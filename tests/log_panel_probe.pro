QT = core network concurrent widgets
CONFIG += c++11 console
TEMPLATE = app
TARGET = log-panel-probe
INCLUDEPATH += ../apps/envoy ../apps/deployment ../common/experiment
SOURCES += log_panel_probe.cpp ../apps/envoy/servicelogpanel.cpp ../apps/envoy/logtimeline.cpp ../apps/deployment/deploymentcommon.cpp ../common/experiment/strictjson.cpp
HEADERS += ../apps/envoy/servicelogpanel.h
