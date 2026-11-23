QT += core gui widgets network concurrent
CONFIG += c++11
TEMPLATE = app
TARGET = GetDAQ
RESOURCES += ../../third_party/application-icons/getdaq.qrc
win32: RC_ICONS = ../../third_party/application-icons/icons/getdaq.ico
macx: ICON = ../../third_party/application-icons/icons/getdaq.icns
include(core.pri)
SOURCES += main.cpp mainwindow.cpp mainwindow_runcommands.cpp mainwindow_configuration.cpp mainwindow_configpresentation.cpp mainwindow_maintenance.cpp mainwindow_menus.cpp mainwindow_recoverymenus.cpp mainwindow_connections.cpp mainwindow_status.cpp mainwindow_control_state.cpp  mainwindow_log.cpp mainwindow_layout.cpp ipwindow.cpp widgetplot2d.cpp         
SOURCES += consolelogstore.cpp
HEADERS += mainwindow.h ipwindow.h widgetplot2d.h  consolelogstore.h
FORMS += mainwindow.ui
win32-msvc*: QMAKE_CXXFLAGS += /utf-8
include(../../common/build-output.pri)




SOURCES += mainwindow_console.cpp consolepresentation.cpp consolepresentation_icons.cpp consolepresentation_styles.cpp
HEADERS += consolepresentation.h

RESOURCES += consoleicons.qrc

SOURCES += autorunwidget.cpp diagnostichighlighter.cpp
HEADERS += autorunwidget.h diagnostichighlighter.h

include(../waveformviewer/waveformwindow.pri)

SOURCES += mainwindow_tools.cpp
SOURCES += offlinedatatoolswindow.cpp
HEADERS += offlinedatatoolswindow.h

include(originalservices.pri)

SOURCES += mainwindow_services.cpp

SOURCES += applicationinstance.cpp
HEADERS += applicationinstance.h
SOURCES += mainwindow_statemanagement.cpp

SOURCES += coboconnectionwindow.cpp
HEADERS += coboconnectionwindow.h

SOURCES += servicelogpanel.cpp
HEADERS += servicelogpanel.h

SOURCES += logtimeline.cpp
HEADERS += logtimeline.h

SOURCES += userguidewindow.cpp
HEADERS += userguidewindow.h
RESOURCES += userguide.qrc

SOURCES += mainwindow_shutdown.cpp
