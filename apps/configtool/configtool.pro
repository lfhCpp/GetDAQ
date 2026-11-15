QT = core
CONFIG += c++11 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = getdaq-config
include(../../common/config/xcfg.pri)
SOURCES += main.cpp scanplancommands.cpp scanplancommands_support.cpp \
    scanplancommands_metadata.cpp scanplancommands_generation.cpp scanplancommands_publication.cpp \
    scanplancommand_peaking.cpp scanplancommand_pulser.cpp \
    scanplancommand_threshold.cpp scanplancommand_autogain.cpp \
    configtool_registercommands.cpp configtool_registercommands_options.cpp
include(../../common/build-output.pri)
win32-msvc*: QMAKE_CXXFLAGS += /utf-8
