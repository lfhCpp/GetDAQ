QT += core network concurrent
QT -= gui
CONFIG += c++11 console
TEMPLATE = app
TARGET = native-gui-probe
include(../apps/envoy/core.pri)
SOURCES += native_gui_probe.cpp

