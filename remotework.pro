# --- 项目与模板 ---
TEMPLATE  = app
TARGET    = remotework
CONFIG   += qt widgets console c++17

# --- 使用的 Qt 模块 ---
QT += core gui widgets sql

# --- 源码/头文件/UI 表单 ---
SOURCES += \
    database.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    database.h \
    mainwindow.h

FORMS += \
    mainwindow.ui

# --- C++ 标准与编译警告（可选但推荐） ---
QMAKE_CXXFLAGS += -Wall -Wextra

# --- 资源（如果暂时没有，可删除本段） ---
# RESOURCES += resources.qrc

# --- 高 DPI 支持（Qt 5 常用，Qt 6 默认即可） ---
greaterThan(QT_MAJOR_VERSION, 5) {
    QT += printsupport
} else {
    DEFINES += QT_DEPRECATED_WARNINGS
}

# --- 平台细节（可选） ---
win32 {
    # Windows 平台特定设置放这里
}
unix:!macx {
    # Linux 平台特定设置放这里
}
macx {
    # macOS 平台特定设置放这里
}

