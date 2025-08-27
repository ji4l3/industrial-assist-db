#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QSqlTableModel>
#include <QSqlDatabase>
#include <QSqlError>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , model(nullptr)
{
    ui->setupUi(this);

    // 1. 建立一个基于 users 表的 model
    model = new QSqlTableModel(this, QSqlDatabase::database());
    model->setTable("users");
    model->setEditStrategy(QSqlTableModel::OnFieldChange);  // 单元格改了就立即写入数据库
    model->select();  // 加载数据

    // 2. 绑定到 QTableView
    ui->tableView->setModel(model);

    // 3. 可选：设置表头
    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "用户名");
    model->setHeaderData(2, Qt::Horizontal, "密码哈希");
    model->setHeaderData(3, Qt::Horizontal, "姓名");
    model->setHeaderData(4, Qt::Horizontal, "角色");
    model->setHeaderData(5, Qt::Horizontal, "电话");
    model->setHeaderData(6, Qt::Horizontal, "邮箱");
    model->setHeaderData(7, Qt::Horizontal, "创建时间");
}

MainWindow::~MainWindow()
{
    delete ui;
}



