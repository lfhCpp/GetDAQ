QT += core network testlib concurrent widgets
CONFIG += c++11 testcase console
TEMPLATE = app
TARGET = getdaq-tests
include(../apps/envoy/core.pri)
SOURCES += product_tests.cpp mocknode_fixture.cpp getdaq_test_support.cpp waveformviewer_tests.cpp waveformviewer_testfixtures.cpp
HEADERS += mocknode_fixture_p.h getdaq_test_support_p.h waveformviewer_tests_p.h waveformviewer_testfixtures_p.h
SOURCES += $$files($$PWD/../apps/envoy/mainwindow*.cpp)
SOURCES += ../apps/envoy/ipwindow.cpp ../apps/envoy/widgetplot2d.cpp ../apps/envoy/consolelogstore.cpp
SOURCES += ../apps/envoy/consolepresentation.cpp ../apps/envoy/consolepresentation_icons.cpp ../apps/envoy/consolepresentation_styles.cpp
SOURCES += ../apps/envoy/autorunwidget.cpp ../apps/envoy/diagnostichighlighter.cpp ../apps/envoy/offlinedatatoolswindow.cpp ../apps/envoy/applicationinstance.cpp
HEADERS += ../apps/envoy/mainwindow.h ../apps/envoy/ipwindow.h ../apps/envoy/widgetplot2d.h ../apps/envoy/consolelogstore.h ../apps/envoy/autorunwidget.h ../apps/envoy/diagnostichighlighter.h ../apps/envoy/offlinedatatoolswindow.h
FORMS += ../apps/envoy/mainwindow.ui
RESOURCES += ../apps/envoy/consoleicons.qrc
include(../apps/waveformviewer/waveformwindow.pri)
include(../apps/envoy/originalservices.pri)

SOURCES += ../apps/envoy/coboconnectionwindow.cpp
HEADERS += ../apps/envoy/coboconnectionwindow.h

SOURCES += ../apps/envoy/servicelogpanel.cpp
HEADERS += ../apps/envoy/servicelogpanel.h

SOURCES += ../apps/sentry/sentryservice_status.cpp
INCLUDEPATH += ../apps/sentry
