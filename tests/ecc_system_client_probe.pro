QT += core network
QT -= gui
CONFIG += c++11 console
TEMPLATE = app
TARGET = ecc-system-client-probe
include(../common/experiment/experiment.pri)
SOURCES += ../common/config/configurationrange.cpp ../common/config/configurationrange_parse.cpp
include(../common/control/control.pri)
INCLUDEPATH += ../apps/envoy
SOURCES += ecc_system_client_probe.cpp ../apps/envoy/httpclient.cpp ../apps/envoy/eccsystemclient.cpp ../apps/envoy/eccsystemclient_operations.cpp ../apps/envoy/eccsystemclient_observations.cpp ../apps/envoy/eccsystemclient_control.cpp ../apps/envoy/eccsystemclient_maintenance.cpp
HEADERS += ../apps/envoy/httpclient.h ../apps/envoy/eccsystemclient.h ../apps/envoy/eccsystemclient_p.h
