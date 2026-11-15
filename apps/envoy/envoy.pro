QT += core gui widgets network concurrent
CONFIG += c++11
TEMPLATE = app
TARGET = GetDAQ
SOURCES += hardwarestatewindow.cpp
HEADERS += hardwarestatewindow.h
FORMS += hardwarestatewindow.ui
include(core.pri)
SOURCES += main.cpp mainwindow.cpp mainwindow_runcommands.cpp mainwindow_configuration.cpp mainwindow_configpresentation.cpp mainwindow_maintenance.cpp mainwindow_menus.cpp mainwindow_recoverymenus.cpp mainwindow_connections.cpp mainwindow_status.cpp mainwindow_control_state.cpp mainwindow_observations.cpp mainwindow_log.cpp mainwindow_layout.cpp ipwindow.cpp widgetplot2d.cpp configwindow.cpp configvaluedelegate.cpp configwindow_valueediting.cpp configwindow_tree.cpp configwindow_history.cpp configwindow_xml_export.cpp configwindow_workspacecopy.cpp configwindow_snapshot.cpp configwindow_structure.cpp
SOURCES += consolelogstore.cpp
HEADERS += mainwindow.h ipwindow.h widgetplot2d.h configwindow.h consolelogstore.h
FORMS += mainwindow.ui
SOURCES += configurationworkspacewindow.cpp
HEADERS += configurationworkspacewindow.h
FORMS += configurationworkspacewindow.ui
SOURCES += workspacestructurewindow.cpp
HEADERS += workspacestructurewindow.h
FORMS += workspacestructurewindow.ui
SOURCES += workspacerecoverywindow.cpp
HEADERS += workspacerecoverywindow.h
FORMS += workspacerecoverywindow.ui
SOURCES += workspacemigrationwindow.cpp
HEADERS += workspacemigrationwindow.h
FORMS += workspacemigrationwindow.ui
SOURCES += configurationreadbackwindow.cpp
HEADERS += configurationreadbackwindow.h
FORMS += configurationreadbackwindow.ui
SOURCES += transferwindow.cpp
HEADERS += transferwindow.h
FORMS += transferwindow.ui
win32-msvc*: QMAKE_CXXFLAGS += /utf-8
include(../../common/build-output.pri)

SOURCES += configurationrepositorywindow.cpp
HEADERS += configurationrepositorywindow.h
SOURCES += hardwaredescriptionwindow.cpp
HEADERS += hardwaredescriptionwindow.h

SOURCES += binaryconfigurationwindow.cpp
HEADERS += binaryconfigurationwindow.h
FORMS += binaryconfigurationwindow.ui
SOURCES += plantransitionwindow.cpp plantransitionwindow_completion.cpp
HEADERS += plantransitionwindow.h
FORMS += plantransitionwindow.ui
SOURCES += cobomaintenancewindow.cpp
HEADERS += cobomaintenancewindow.h
FORMS += cobomaintenancewindow.ui

SOURCES += coldhandoffwindow.cpp
HEADERS += coldhandoffwindow.h
FORMS += coldhandoffwindow.ui
SOURCES += failedhandoffwindow.cpp
HEADERS += failedhandoffwindow.h
FORMS += failedhandoffwindow.ui
SOURCES += scanplanwindow.cpp scanplanwindow_ui.cpp scanplanwindow_formui.cpp scanplanwindow_policyui.cpp scanplanwindow_runtimeui.cpp scanplanwindow_journalui.cpp scanplanwindow_process.cpp scanplanwindow_processoperations.cpp scanplanwindow_processlifecycle.cpp scanplanwindow_policy.cpp scanplanwindow_policystate.cpp scanplanwindow_creation.cpp scanplanwindow_runtime.cpp scanplanwindow_observation.cpp
HEADERS += scanplanwindow.h
SOURCES += scanplanoutput.cpp
HEADERS += scanplanoutput.h
SOURCES += scanplanreview.cpp scanplanreview_record.cpp scanplanreview_snapshot.cpp scanplanreview_binding.cpp
HEADERS += scanplanreview.h scanplanreview_p.h

SOURCES += mainwindow_console.cpp consolepresentation.cpp consolepresentation_icons.cpp consolepresentation_styles.cpp
HEADERS += consolepresentation.h

RESOURCES += consoleicons.qrc

SOURCES += autorunwidget.cpp diagnostichighlighter.cpp
HEADERS += autorunwidget.h diagnostichighlighter.h

include(../waveformviewer/waveformwindow.pri)

SOURCES += mainwindow_tools.cpp
SOURCES += offlinedatatoolswindow.cpp
HEADERS += offlinedatatoolswindow.h

include(hardwaresetup.pri)

SOURCES += mainwindow_services.cpp

SOURCES += applicationinstance.cpp
HEADERS += applicationinstance.h
SOURCES += mainwindow_statemanagement.cpp
