QT += core network
QT -= gui
CONFIG += c++11 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = getdaq-config-upload
include(../../common/build-output.pri)
include(../../common/experiment/experiment.pri)
INCLUDEPATH += ../../common/config ../envoy
SOURCES += ../../common/config/configurationrange.cpp ../../common/config/configurationrange_parse.cpp ../envoy/configurationrepositoryclient.cpp ../envoy/configurationrepositoryclient_upload.cpp ../envoy/httpclient.cpp selectionfile.cpp main.cpp
HEADERS += ../envoy/configurationrepositoryclient.h ../envoy/httpclient.h selectionfile.h
strict_c++: QMAKE_CXXFLAGS += -pedantic-errors
