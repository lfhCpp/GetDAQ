QT += core network concurrent
QT -= gui
CONFIG += c++11 console
TEMPLATE = app
TARGET = getdaq-node-agent
include(deploymentcore.pri)
include(../../common/config/xcfg.pri)
INCLUDEPATH += ../envoy ../../third_party/yaml-cpp/include ../../third_party/yaml-cpp/src
DEFINES += YAML_CPP_STATIC_DEFINE
SOURCES += nodeagentmain.cpp nodeagent.cpp ../envoy/localserviceprocesses.cpp ../envoy/runconfig.cpp ../envoy/runconfig_yaml.cpp ../envoy/autorun.cpp ../../common/http/httpserver.cpp
SOURCES += $$files($$PWD/../../third_party/yaml-cpp/src/*.cpp) $$files($$PWD/../../third_party/yaml-cpp/src/contrib/*.cpp)
HEADERS += nodeagent.h ../../common/http/httpserver.h

SOURCES += nodeagentoriginal.cpp

SOURCES += nodeagentproject.cpp
