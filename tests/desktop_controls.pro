QT += core gui widgets network concurrent testlib
CONFIG += c++11 testcase console
TEMPLATE = app
TARGET = desktop-controls-tests
include(../apps/envoy/core.pri)
SOURCES -= $$clean_path($$PWD/../apps/envoy/cobomaintenancewindow_actions.cpp)
SOURCES += ../apps/envoy/localserviceswindow.cpp test_desktop_controls.cpp waveformviewer_testfixtures.cpp
HEADERS += ../apps/envoy/localserviceswindow.h
include(../apps/waveformviewer/waveformwindow.pri)
