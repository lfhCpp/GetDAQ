QT += core network
QT -= gui
CONFIG += c++11 console
TEMPLATE = app
TARGET = cold-http-response-probe
INCLUDEPATH += ../apps/envoy ../common/http
SOURCES += cold_http_response_probe.cpp ../apps/envoy/httpclient.cpp ../common/http/httpserver.cpp
HEADERS += ../apps/envoy/httpclient.h ../common/http/httpserver.h
