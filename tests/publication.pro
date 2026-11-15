QT += core testlib
QT -= gui
CONFIG += c++11 testcase console
TEMPLATE = app
TARGET = configuration-publication-tests
include(../common/config/xcfg.pri)
SOURCES += test_configuration_publication.cpp
strict_c++: QMAKE_CXXFLAGS += -pedantic-errors
