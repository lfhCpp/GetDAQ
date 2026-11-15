QT = core
CONFIG += c++11 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = getdaq-config-probe
include(../common/config/xcfg.pri)
SOURCES += ../apps/configtool/main.cpp ../apps/configtool/scanplancommands.cpp \
    ../apps/configtool/scanplancommands_support.cpp \
    ../apps/configtool/scanplancommands_metadata.cpp \
    ../apps/configtool/scanplancommands_generation.cpp \
    ../apps/configtool/scanplancommands_publication.cpp \
    ../apps/configtool/scanplancommand_peaking.cpp \
    ../apps/configtool/scanplancommand_pulser.cpp \
    ../apps/configtool/scanplancommand_threshold.cpp \
    ../apps/configtool/scanplancommand_autogain.cpp \
    ../apps/configtool/configtool_registercommands.cpp \
    ../apps/configtool/configtool_registercommands_options.cpp
strict_c++: QMAKE_CXXFLAGS += -pedantic-errors
