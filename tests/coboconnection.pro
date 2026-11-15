QT += core gui widgets network testlib
CONFIG += c++11 testcase console
TEMPLATE = app
TARGET = cobo-connection-tests
SOURCES += test_coboconnection.cpp ../apps/envoy/coboconnectionwindow.cpp
HEADERS += ../apps/envoy/coboconnectionwindow.h
INCLUDEPATH += ../apps/envoy
