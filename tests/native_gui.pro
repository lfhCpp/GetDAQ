QT += core network testlib concurrent
QT -= gui
CONFIG += c++11 testcase console
TEMPLATE = app
TARGET = native-gui-tests
include(../apps/envoy/core.pri)
include(../common/http/httpserver.pri)
SOURCES += test_native_gui.cpp
