#include "mainwindow.h"
#include <QApplication>
#include <QSqlDatabase>
#include <QDebug>
#include "database.h"      // << 新增

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 打印驱动确认 QSQLITE 存在
    qDebug() << "Drivers:" << QSqlDatabase::drivers();

    // 初始化数据库（建库+建表+PRAGMA）
    if (!DB::init()) {
        qWarning() << "Database init failed, app will still start for UI check.";
    }

    MainWindow w;
    w.show();
    return a.exec();
}


