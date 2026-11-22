QT = core network concurrent
CONFIG += c++11 console
TEMPLATE = app
TARGET = unified-project-controller-probe
include(../apps/deployment/deploymentcore.pri)
INCLUDEPATH += ../apps/envoy ../third_party/yaml-cpp/include ../third_party/yaml-cpp/src
DEFINES += YAML_CPP_STATIC_DEFINE
SOURCES += unified_project_controller_probe.cpp ../apps/deployment/deploymentcontroller.cpp ../apps/envoy/localserviceprocesses.cpp ../apps/envoy/runconfig.cpp ../apps/envoy/runconfig_yaml.cpp ../apps/envoy/autorun.cpp
SOURCES += $$files($$PWD/../third_party/yaml-cpp/src/*.cpp) $$files($$PWD/../third_party/yaml-cpp/src/contrib/*.cpp)
INCLUDEPATH += ../common/experiment
SOURCES += ../common/experiment/strictjson.cpp ../common/experiment/configurationsha256.cpp

SOURCES += ../apps/envoy/backendshutdown.cpp
