QT =
CONFIG += c++11 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = getdaq-config-check
include(../../common/experiment/experiment.pri)
INCLUDEPATH += ../../common/config
SOURCES += ../../common/config/configurationrange.cpp ../../common/config/configurationrange_parse.cpp main.cpp artifactcommands.cpp scancommands.cpp
HEADERS += artifactcommands.h scancommands.h
include(../../common/build-output.pri)
