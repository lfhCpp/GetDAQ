QT += core network concurrent testlib
QT -= gui
CONFIG += c++11 console testcase
TEMPLATE = app
TARGET = run-file-policy-tests
INCLUDEPATH += ../apps/sentry ../apps/envoy ../common/http
include(../common/http/httpserver.pri)
SOURCES += run_file_policy_tests.cpp sentry_service_tests.cpp ../apps/envoy/httpclient.cpp
SOURCES += ../apps/sentry/sentryconfig.cpp ../apps/sentry/sentryservice.cpp ../apps/sentry/sentryservice_http.cpp ../apps/sentry/sentryservice_http_validation.cpp ../apps/sentry/sentryservice_status.cpp ../apps/sentry/sentryservice_catalog.cpp ../apps/sentry/sentryservice_replacement.cpp ../apps/sentry/sentrycatalog_helpers.cpp ../apps/sentry/sentrycatalog_seal.cpp ../apps/envoy/localserviceprocesses.cpp
HEADERS += ../apps/sentry/sentryservice.h ../apps/envoy/httpclient.h
