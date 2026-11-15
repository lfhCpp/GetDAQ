QT += core network testlib
QT -= gui
CONFIG += c++11 testcase console
TEMPLATE = app
TARGET = ecc-system-client-tests
include(../common/experiment/experiment.pri)
SOURCES += ../common/config/configurationrange.cpp ../common/config/configurationrange_parse.cpp
include(../common/control/control.pri)
include(../common/http/httpserver.pri)
INCLUDEPATH += ../apps/envoy
SOURCES += test_ecc_system_client.cpp ../apps/envoy/httpclient.cpp ../apps/envoy/eccsystemclient.cpp ../apps/envoy/eccsystemclient_operations.cpp ../apps/envoy/eccsystemclient_observations.cpp ../apps/envoy/eccsystemclient_control.cpp ../apps/envoy/eccsystemclient_maintenance.cpp
HEADERS += ../apps/envoy/httpclient.h ../apps/envoy/eccsystemclient.h ../apps/envoy/eccsystemclient_p.h
SOURCES += ../apps/envoy/eccclientstore.cpp ../apps/envoy/eccclientstore_state.cpp ../apps/envoy/eccclientstore_queries.cpp ../apps/envoy/eccclientstore_retirement.cpp ../apps/envoy/eccclientstore_mutations.cpp ../apps/envoy/eccclientstore_observationpolicy.cpp ../apps/envoy/eccclientstore_recoverypolicy.cpp ../apps/envoy/eccclientstore_storage.cpp
HEADERS += ../apps/envoy/eccclientstore.h ../apps/envoy/eccclientstore_impl_p.h ../apps/envoy/eccclientstore_observationpolicy.h ../apps/envoy/eccclientstore_recoverypolicy.h
