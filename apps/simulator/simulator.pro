QT += core network concurrent
QT -= gui
CONFIG += c++11 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = getdaq-simulator
include(../../common/http/httpserver.pri)
INCLUDEPATH += ../sentry
SOURCES += main.cpp simulator.cpp simulator_configuration.cpp simulator_control.cpp simulator_data.cpp ../sentry/sentryconfig.cpp ../sentry/sentryservice.cpp ../sentry/sentryservice_http.cpp ../sentry/sentryservice_http_validation.cpp ../sentry/sentryservice_status.cpp ../sentry/sentryservice_catalog.cpp ../sentry/sentryservice_replacement.cpp ../sentry/sentrycatalog_helpers.cpp ../sentry/sentrycatalog_seal.cpp
HEADERS += simulator.h ../sentry/sentryservice.h ../sentry/sentryservice_http_p.h ../sentry/sentrycatalog_p.h
include(../../common/build-output.pri)
win32-msvc*: QMAKE_CXXFLAGS += /utf-8
