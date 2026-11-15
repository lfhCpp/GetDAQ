QT += core gui widgets network concurrent testlib
CONFIG += c++11 testcase console
TEMPLATE = app
TARGET = hardware-setup-tests
include(../apps/envoy/core.pri)
include(../apps/envoy/hardwaresetup.pri)
SOURCES += test_hardwaresetup.cpp ../apps/envoy/ipwindow.cpp
HEADERS += ../apps/envoy/ipwindow.h
DEFINES += GETDAQ_SETUP_SOURCE_ROOT=\\\"$$PWD/..\\\"
SOURCES -= $$clean_path($$PWD/../apps/envoy/cobomaintenancewindow_actions.cpp)

SOURCES += ../apps/envoy/applicationinstance.cpp
HEADERS += ../apps/envoy/applicationinstance.h
