#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QSqlDatabase>

namespace DB {

// 返回数据库文件绝对路径（跨平台 AppData 目录）
QString dbPath();

// 初始化数据库（打开连接、设置 PRAGMA、按版本迁移）
bool init();

// 获取主连接（已命名为 "main"）
QSqlDatabase database();

} // namespace DB

#endif // DATABASE_H
