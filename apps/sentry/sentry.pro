QT += core network concurrent
QT -= gui
CONFIG += c++11 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = getdaq-sentry
include(../../common/http/httpserver.pri)
SOURCES += main.cpp sentryconfig.cpp sentryservice.cpp sentryservice_http.cpp sentryservice_http_validation.cpp sentryservice_status.cpp sentryservice_catalog.cpp sentryservice_replacement.cpp sentrycatalog_helpers.cpp sentrycatalog_seal.cpp
HEADERS += sentryservice.h sentryservice_http_p.h sentrycatalog_p.h
include(../../common/build-output.pri)
win32-msvc*: QMAKE_CXXFLAGS += /utf-8
