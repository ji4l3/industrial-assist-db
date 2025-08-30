#include "database.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

namespace {
constexpr const char* kConnName = "main";

bool execOrLog(QSqlQuery& q, const char* sql) {
    if (!q.exec(sql)) {
        qDebug() << "[SQL ERROR]" << q.lastError().text() << "SQL:" << sql;
        return false;
    }
    return true;
}

bool setPragmas(QSqlDatabase& db) {
    QSqlQuery q(db);
    return
        execOrLog(q, "PRAGMA foreign_keys = ON;") &&
        execOrLog(q, "PRAGMA journal_mode = WAL;") &&
        execOrLog(q, "PRAGMA synchronous = NORMAL;") &&
        execOrLog(q, "PRAGMA busy_timeout = 5000;") &&          // 写锁等待
        execOrLog(q, "PRAGMA wal_autocheckpoint = 1000;");      // 适度 checkpoint
}

int currentSchemaVersion(QSqlDatabase& db) {
    QSqlQuery q(db);
    if (!q.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='schema_migrations';"))
        return 0;
    if (!q.next()) return 0;

    if (!q.exec("SELECT COALESCE(MAX(version),0) FROM schema_migrations;"))
        return 0;
    if (q.next()) return q.value(0).toInt();
    return 0;
}

bool begin(QSqlDatabase& db)    { return db.transaction(); }
bool commit(QSqlDatabase& db)   { return db.commit(); }
bool rollback(QSqlDatabase& db) { return db.rollback(); }

} // anonymous

namespace DB {

QString dbPath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/industrial_assist.db";
}

QSqlDatabase database() {
    return QSqlDatabase::database(kConnName);
}

bool init() {
    // 1) 打开/创建命名连接
    QSqlDatabase db = QSqlDatabase::contains(kConnName)
        ? QSqlDatabase::database(kConnName)
        : QSqlDatabase::addDatabase("QSQLITE", kConnName);

    db.setDatabaseName(dbPath());
    if (!db.open()) {
        qCritical() << "Open DB failed:" << db.lastError().text();
        return false;
    }

    if (!setPragmas(db)) {
        qCritical() << "Set PRAGMA failed";
        return false;
    }

    // 2) 初始化迁移表（若不存在）
    {
        QSqlQuery q(db);
        if (!execOrLog(q, "CREATE TABLE IF NOT EXISTS schema_migrations (version INTEGER PRIMARY KEY);"))
            return false;
    }

    int ver = currentSchemaVersion(db);

    // =========== v1 基础结构 ===========
    if (ver < 1) {
        if (!begin(db)) { qDebug() << "Begin v1 failed:" << db.lastError().text(); return false; }
        QSqlQuery q(db);

        const char* v1[] = {
            // users
            "CREATE TABLE IF NOT EXISTS users ("
            "  user_id       INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  username      TEXT NOT NULL UNIQUE,"
            "  password_hash TEXT NOT NULL,"
            "  full_name     TEXT,"
            "  role          TEXT NOT NULL CHECK(role IN ('expert','requester','admin')),"
            "  phone         TEXT,"
            "  email         TEXT,"
            "  created_at    INTEGER DEFAULT (strftime('%s','now'))"
            ");",

            // devices
            "CREATE TABLE IF NOT EXISTS devices ("
            "  device_id     INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  sn            TEXT NOT NULL UNIQUE,"
            "  model         TEXT,"
            "  location      TEXT,"
            "  owner_org     TEXT,"
            "  created_at    INTEGER DEFAULT (strftime('%s','now'))"
            ");",

            // work_orders
            "CREATE TABLE IF NOT EXISTS work_orders ("
            "  order_id      INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  requester_id  INTEGER NOT NULL REFERENCES users(user_id) ON DELETE RESTRICT,"
            "  expert_id     INTEGER     REFERENCES users(user_id) ON DELETE SET NULL,"
            "  device_id     INTEGER NOT NULL REFERENCES devices(device_id) ON DELETE RESTRICT,"
            "  status        TEXT NOT NULL CHECK(status IN ('open','assigned','in_progress','paused','resolved','closed','canceled')),"
            "  priority      INTEGER NOT NULL DEFAULT 3 CHECK(priority BETWEEN 1 AND 5),"
            "  title         TEXT NOT NULL,"
            "  description   TEXT,"
            "  created_at    INTEGER DEFAULT (strftime('%s','now')),"
            "  assigned_at   INTEGER,"
            "  closed_at     INTEGER"
            ");",

            // device_data
            "CREATE TABLE IF NOT EXISTS device_data ("
            "  data_id       INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  device_id     INTEGER NOT NULL REFERENCES devices(device_id) ON DELETE CASCADE,"
            "  ts            INTEGER NOT NULL,"
            "  metric        TEXT NOT NULL,"   // e.g. temperature / pressure
            "  value_num     REAL,"
            "  value_text    TEXT,"
            "  UNIQUE(device_id, ts, metric)"
            ");",

            // order_logs
            "CREATE TABLE IF NOT EXISTS order_logs ("
            "  log_id        INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  order_id      INTEGER NOT NULL REFERENCES work_orders(order_id) ON DELETE CASCADE,"
            "  actor_id      INTEGER     REFERENCES users(user_id) ON DELETE SET NULL,"
            "  action        TEXT NOT NULL,"
            "  content       TEXT,"
            "  created_at    INTEGER DEFAULT (strftime('%s','now'))"
            ");",

            // basic indexes
            "CREATE INDEX IF NOT EXISTS idx_work_orders_status       ON work_orders(status);",
            "CREATE INDEX IF NOT EXISTS idx_device_data_device_ts    ON device_data(device_id, ts);",

            nullptr
        };

        for (int i = 0; v1[i]; ++i) { if (!execOrLog(q, v1[i])) { rollback(db); return false; } }
        if (!execOrLog(q, "INSERT INTO schema_migrations(version) VALUES(1);")) { rollback(db); return false; }
        if (!commit(db)) { qDebug() << "Commit v1 failed:" << db.lastError().text(); return false; }
        ver = 1;
    }

    // =========== v2 视频会议 ===========
    if (ver < 2) {
        if (!begin(db)) { qDebug() << "Begin v2 failed:" << db.lastError().text(); return false; }
        QSqlQuery q(db);

        const char* v2[] = {
            "CREATE TABLE IF NOT EXISTS video_meetings ("
            "  meeting_id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  order_id   INTEGER NOT NULL REFERENCES work_orders(order_id) ON DELETE CASCADE,"
            "  start_time INTEGER NOT NULL,"
            "  end_time   INTEGER,"
            "  topic      TEXT,"
            "  created_at INTEGER DEFAULT (strftime('%s','now'))"
            ");",

            "CREATE TABLE IF NOT EXISTS meeting_participants ("
            "  participant_id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  meeting_id     INTEGER NOT NULL REFERENCES video_meetings(meeting_id) ON DELETE CASCADE,"
            "  user_id        INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,"
            "  role           TEXT,"
            "  joined_at      INTEGER DEFAULT (strftime('%s','now')),"
            "  UNIQUE(meeting_id, user_id)"
            ");",

            nullptr
        };

        for (int i = 0; v2[i]; ++i) { if (!execOrLog(q, v2[i])) { rollback(db); return false; } }
        if (!execOrLog(q, "INSERT INTO schema_migrations(version) VALUES(2);")) { rollback(db); return false; }
        if (!commit(db)) { qDebug() << "Commit v2 failed:" << db.lastError().text(); return false; }
        ver = 2;
    }

    // =========== v3 索引增强 ===========
    if (ver < 3) {
        if (!begin(db)) { qDebug() << "Begin v3 failed:" << db.lastError().text(); return false; }
        QSqlQuery q(db);

        const char* v3[] = {
            "CREATE INDEX IF NOT EXISTS idx_work_orders_requester ON work_orders(requester_id);",
            "CREATE INDEX IF NOT EXISTS idx_work_orders_expert    ON work_orders(expert_id);",
            "CREATE INDEX IF NOT EXISTS idx_work_orders_device    ON work_orders(device_id);",
            "CREATE INDEX IF NOT EXISTS idx_order_logs_order      ON order_logs(order_id);",
            "CREATE INDEX IF NOT EXISTS idx_order_logs_actor      ON order_logs(actor_id);",
            "CREATE INDEX IF NOT EXISTS idx_video_meetings_order  ON video_meetings(order_id);",
            "CREATE INDEX IF NOT EXISTS idx_meeting_participants_mid ON meeting_participants(meeting_id);",
            "CREATE INDEX IF NOT EXISTS idx_meeting_participants_uid ON meeting_participants(user_id);",
            "CREATE INDEX IF NOT EXISTS idx_device_data_dev_metric_ts ON device_data(device_id, metric, ts);",
            nullptr
        };

        for (int i = 0; v3[i]; ++i) { if (!execOrLog(q, v3[i])) { rollback(db); return false; } }
        if (!execOrLog(q, "INSERT INTO schema_migrations(version) VALUES(3);")) { rollback(db); return false; }
        if (!commit(db)) { qDebug() << "Commit v3 failed:" << db.lastError().text(); return false; }
        ver = 3;
    }

    // =========== v4 聊天消息 ===========
    if (ver < 4) {
        if (!begin(db)) { qDebug() << "Begin v4 failed:" << db.lastError().text(); return false; }
        QSqlQuery q(db);

        const char* v4[] = {
            "CREATE TABLE IF NOT EXISTS chat_messages ("
            "  message_id   INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  order_id     INTEGER NOT NULL REFERENCES work_orders(order_id) ON DELETE CASCADE,"
            "  sender_id    INTEGER NOT NULL REFERENCES users(user_id)        ON DELETE SET NULL,"
            "  receiver_id  INTEGER     REFERENCES users(user_id)             ON DELETE SET NULL,"
            "  content      TEXT,"
            "  content_type TEXT DEFAULT 'text',"
            "  attachment   TEXT,"
            "  is_read      INTEGER DEFAULT 0,"
            "  created_at   INTEGER DEFAULT (strftime('%s','now')),"
            "  read_at      INTEGER,"
            "  edited_at    INTEGER,"
            "  deleted      INTEGER DEFAULT 0"
            ");",

            "CREATE INDEX IF NOT EXISTS idx_chat_order_time     ON chat_messages(order_id, created_at);",
            "CREATE INDEX IF NOT EXISTS idx_chat_sender_time    ON chat_messages(sender_id, created_at);",
            "CREATE INDEX IF NOT EXISTS idx_chat_receiver_time  ON chat_messages(receiver_id, created_at);",
            nullptr
        };

        for (int i = 0; v4[i]; ++i) {
            if (!execOrLog(q, v4[i])) { rollback(db); return false; }
        }
        if (!execOrLog(q, "INSERT INTO schema_migrations(version) VALUES(4);")) { rollback(db); return false; }
        if (!commit(db)) { qDebug() << "Commit v4 failed:" << db.lastError().text(); return false; }
        ver = 4;
    }

    // =========== v5 聊天触发器 ===========
    if (ver < 5) {
        if (!begin(db)) { qDebug() << "Begin v5 failed:" << db.lastError().text(); return false; }
        QSqlQuery q(db);

        const char* v5[] = {
            "CREATE TRIGGER IF NOT EXISTS trg_chat_sender_valid "
            "BEFORE INSERT ON chat_messages "
            "BEGIN "
            "  SELECT CASE WHEN NOT EXISTS ("
            "    SELECT 1 FROM work_orders "
            "    WHERE order_id = NEW.order_id "
            "      AND (requester_id = NEW.sender_id OR expert_id = NEW.sender_id)"
            "  ) "
            "  THEN RAISE(ABORT, 'Sender not a participant of this work order') END;"
            "END;",

            nullptr
        };

        for (int i = 0; v5[i]; ++i) {
            if (!execOrLog(q, v5[i])) { rollback(db); return false; }
        }
        if (!execOrLog(q, "INSERT INTO schema_migrations(version) VALUES(5);")) { rollback(db); return false; }
        if (!commit(db)) { qDebug() << "Commit v5 failed:" << db.lastError().text(); return false; }
        ver = 5;
    }


    qDebug() << "[DB READY]" << dbPath() << "schema version =" << ver;
    return true;
}

} // namespace DB
