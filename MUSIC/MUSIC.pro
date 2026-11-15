#-------------------------------------------------
#
# Project created by QtCreator 2023-07-12T09:48:21
#
#-------------------------------------------------

QT       += core gui
QT       += core gui network

#QMAKE_LFLAGS += -Wl,--large-address-aware#Qt4：允许进程积累超过 2GB 的内存

#Qt5:允许使用高位内存
#QMAKE_LFLAGS_WINDOWS += /LARGEADDRESSAWARE

CONFIG+=resources_big

QT += widgets printsupport#示波器功能相关库

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = MUSIC
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp \
        mainwindow.cpp \
    ipwindow.cpp \
    cmdwindow.cpp \
    multithread.cpp \
    qcustomplot.cpp \
    widgetplot2d.cpp \
    analyse.cpp \
    savethread.cpp

HEADERS += \
        mainwindow.h \
    ipwindow.h \
    cmdwindow.h \
    multithread.h \
    qcustomplot.h \
    widgetplot2d.h \
    analyse.h \
    savethread.h

FORMS += \
        mainwindow.ui \
    ipwindow.ui \
    cmdwindow.ui \
    widgetplot2d.ui

#RC_ICONS =MUSIC.png

#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../work/root/lib/release/ -lTree
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../work/root/lib/debug/ -lTree
#else:unix: LIBS += -L$$PWD/../../work/root/lib/ -lTree

#INCLUDEPATH += $$PWD/../../work/root/include
#DEPENDPATH += $$PWD/../../work/root/include

#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../work/root/lib/release/ -lCore
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../work/root/lib/debug/ -lCore
#else:unix: LIBS += -L$$PWD/../../work/root/lib/ -lCore

#INCLUDEPATH += $$PWD/../../work/root/include
#DEPENDPATH += $$PWD/../../work/root/include

#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../work/root/lib/release/ -lHist
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../work/root/lib/debug/ -lHist
#else:unix: LIBS += -L$$PWD/../../work/root/lib/ -lHist

#INCLUDEPATH += $$PWD/../../work/root/include
#DEPENDPATH += $$PWD/../../work/root/include

#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../work/root/lib/release/ -lXrdFileCache-5
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../work/root/lib/debug/ -lXrdFileCache-5
#else:unix: LIBS += -L$$PWD/../../work/root/lib/ -lXrdFileCache-5

#INCLUDEPATH += $$PWD/../../work/root/include
#DEPENDPATH += $$PWD/../../work/root/include

RESOURCES += \
    image.qrc

DISTFILES +=

include(../apps/envoy/hardwaresetup.pri)
