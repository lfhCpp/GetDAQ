QT += core network concurrent
CONFIG += c++11
include($$PWD/../../common/config/xcfg.pri)
INCLUDEPATH += $$PWD $$PWD/../../third_party/yaml-cpp/include $$PWD/../../third_party/yaml-cpp/src
DEFINES += YAML_CPP_STATIC_DEFINE
SOURCES += $$PWD/runconfig.cpp $$PWD/runconfig_yaml.cpp $$PWD/protocol.cpp $$PWD/httpclient.cpp \
 $$PWD/eccclient.cpp $$PWD/sentryclient.cpp $$PWD/runcontrol.cpp $$PWD/runcontrol_observation.cpp \
 $$PWD/runcontrol_commands.cpp $$PWD/runcontrol_execution.cpp $$PWD/runcontrol_finalization.cpp \
 $$PWD/runcontrol_pause.cpp $$PWD/runcontrol_autorun.cpp $$PWD/runrecorder.cpp \
 $$PWD/runrecorder_snapshot.cpp $$PWD/runrecorder_replacement.cpp $$PWD/autorun.cpp $$PWD/diagnostics.cpp
HEADERS += $$PWD/runconfig.h $$PWD/protocol.h $$PWD/httpclient.h $$PWD/eccclient.h $$PWD/sentryclient.h \
 $$PWD/runcontrol.h $$PWD/runrecorder.h $$PWD/runrecorder_p.h $$PWD/autorun.h $$PWD/diagnostics.h
SOURCES += $$files($$PWD/../../third_party/yaml-cpp/src/*.cpp) $$files($$PWD/../../third_party/yaml-cpp/src/contrib/*.cpp)

SOURCES += $$PWD/exportersources.cpp
HEADERS += $$PWD/exportersources.h
