QT += core network testlib concurrent widgets
CONFIG += c++11 testcase console
TEMPLATE = app
TARGET = getdaq-tests
SOURCES += ../apps/envoy/hardwarestatewindow.cpp
SOURCES += ../apps/envoy/consolelogstore.cpp
HEADERS += ../apps/envoy/consolelogstore.h
HEADERS += ../apps/envoy/hardwarestatewindow.h
FORMS += ../apps/envoy/hardwarestatewindow.ui
include(../apps/envoy/core.pri)
SOURCES += test_getdaq.cpp waveformviewer_tests.cpp waveformviewer_testfixtures.cpp autorun_tests.cpp consolelayout_tests.cpp offline_tools_tests.cpp native_recovery_policy_tests.cpp configuration_compiler_tests.cpp mocknode_fixture.cpp legacy_runcontrol_tests.cpp getdaq_test_support.cpp scanplan_builder_gui_tests.cpp native_scan_run_tests.cpp native_gui_tests.cpp scanplan_output_tests.cpp native_status_tests.cpp operation_log_tests.cpp soap_protocol_tests.cpp sentry_service_tests.cpp runrecorder_tests.cpp ../apps/envoy/mainwindow.cpp ../apps/envoy/mainwindow_runcommands.cpp ../apps/envoy/mainwindow_configuration.cpp ../apps/envoy/mainwindow_configpresentation.cpp ../apps/envoy/mainwindow_maintenance.cpp ../apps/envoy/mainwindow_menus.cpp ../apps/envoy/mainwindow_recoverymenus.cpp ../apps/envoy/mainwindow_connections.cpp ../apps/envoy/mainwindow_status.cpp ../apps/envoy/mainwindow_control_state.cpp ../apps/envoy/mainwindow_observations.cpp ../apps/envoy/mainwindow_log.cpp ../apps/envoy/mainwindow_layout.cpp ../apps/envoy/ipwindow.cpp ../apps/envoy/widgetplot2d.cpp ../apps/envoy/configwindow.cpp ../apps/envoy/configvaluedelegate.cpp ../apps/envoy/configwindow_valueediting.cpp ../apps/envoy/configwindow_tree.cpp ../apps/envoy/configwindow_history.cpp ../apps/envoy/configwindow_xml_export.cpp ../apps/envoy/configwindow_workspacecopy.cpp ../apps/envoy/configwindow_snapshot.cpp ../apps/envoy/configwindow_structure.cpp
HEADERS += waveformviewer_tests_p.h waveformviewer_testfixtures_p.h autorun_tests_p.h consolelayout_tests_p.h offline_tools_tests_p.h native_recovery_policy_tests_p.h configuration_compiler_tests_p.h mocknode_fixture_p.h legacy_runcontrol_tests_p.h getdaq_test_support_p.h scanplan_builder_gui_tests_p.h native_scan_run_tests_p.h native_gui_tests_p.h scanplan_output_tests_p.h native_status_tests_p.h operation_log_tests_p.h soap_protocol_tests_p.h sentry_service_tests_p.h runrecorder_tests_p.h
include(../apps/waveformviewer/waveformwindow.pri)
SOURCES += ../apps/envoy/offlinedatatoolswindow.cpp
HEADERS += ../apps/envoy/offlinedatatoolswindow.h
SOURCES += ../apps/configtransfer/selectionfile.cpp
HEADERS += ../apps/configtransfer/selectionfile.h
HEADERS += ../apps/envoy/mainwindow.h ../apps/envoy/ipwindow.h ../apps/envoy/widgetplot2d.h ../apps/envoy/configwindow.h
FORMS += ../apps/envoy/mainwindow.ui
SOURCES += ../apps/envoy/configurationworkspacewindow.cpp
HEADERS += ../apps/envoy/configurationworkspacewindow.h
FORMS += ../apps/envoy/configurationworkspacewindow.ui
SOURCES += ../apps/envoy/workspacestructurewindow.cpp
HEADERS += ../apps/envoy/workspacestructurewindow.h
FORMS += ../apps/envoy/workspacestructurewindow.ui
SOURCES += ../apps/envoy/workspacerecoverywindow.cpp
HEADERS += ../apps/envoy/workspacerecoverywindow.h
FORMS += ../apps/envoy/workspacerecoverywindow.ui
SOURCES += ../apps/envoy/workspacemigrationwindow.cpp
HEADERS += ../apps/envoy/workspacemigrationwindow.h
FORMS += ../apps/envoy/workspacemigrationwindow.ui
SOURCES += ../apps/envoy/configurationreadbackwindow.cpp
HEADERS += ../apps/envoy/configurationreadbackwindow.h
FORMS += ../apps/envoy/configurationreadbackwindow.ui
SOURCES += ../apps/envoy/plantransitionwindow.cpp ../apps/envoy/plantransitionwindow_completion.cpp
HEADERS += ../apps/envoy/plantransitionwindow.h
FORMS += ../apps/envoy/plantransitionwindow.ui
SOURCES += ../apps/envoy/transferwindow.cpp
HEADERS += ../apps/envoy/transferwindow.h
FORMS += ../apps/envoy/transferwindow.ui
include(../common/http/httpserver.pri)
INCLUDEPATH += ../apps/sentry ../apps/simulator ../apps/configtransfer
SOURCES += ../apps/sentry/sentryconfig.cpp ../apps/sentry/sentryservice.cpp ../apps/sentry/sentryservice_http.cpp ../apps/sentry/sentryservice_http_validation.cpp ../apps/sentry/sentryservice_status.cpp ../apps/sentry/sentryservice_catalog.cpp ../apps/sentry/sentryservice_replacement.cpp ../apps/sentry/sentrycatalog_helpers.cpp ../apps/sentry/sentrycatalog_seal.cpp ../apps/simulator/simulator.cpp ../apps/simulator/simulator_configuration.cpp ../apps/simulator/simulator_control.cpp ../apps/simulator/simulator_data.cpp
HEADERS += ../apps/sentry/sentryservice.h ../apps/sentry/sentryservice_http_p.h ../apps/sentry/sentrycatalog_p.h ../apps/simulator/simulator.h
win32-msvc*: QMAKE_CXXFLAGS += /utf-8

SOURCES += ../apps/envoy/configurationrepositorywindow.cpp
HEADERS += ../apps/envoy/configurationrepositorywindow.h
SOURCES += ../apps/envoy/hardwaredescriptionwindow.cpp
HEADERS += ../apps/envoy/hardwaredescriptionwindow.h

SOURCES += ../apps/envoy/binaryconfigurationwindow.cpp
HEADERS += ../apps/envoy/binaryconfigurationwindow.h
FORMS += ../apps/envoy/binaryconfigurationwindow.ui
SOURCES += ../apps/envoy/cobomaintenancewindow.cpp
HEADERS += ../apps/envoy/cobomaintenancewindow.h
FORMS += ../apps/envoy/cobomaintenancewindow.ui

SOURCES += ../apps/envoy/coldhandoffwindow.cpp
HEADERS += ../apps/envoy/coldhandoffwindow.h
FORMS += ../apps/envoy/coldhandoffwindow.ui
SOURCES += ../apps/envoy/failedhandoffwindow.cpp
HEADERS += ../apps/envoy/failedhandoffwindow.h
FORMS += ../apps/envoy/failedhandoffwindow.ui
SOURCES += ../apps/envoy/scanplanwindow.cpp ../apps/envoy/scanplanwindow_ui.cpp ../apps/envoy/scanplanwindow_formui.cpp ../apps/envoy/scanplanwindow_policyui.cpp ../apps/envoy/scanplanwindow_runtimeui.cpp ../apps/envoy/scanplanwindow_journalui.cpp ../apps/envoy/scanplanwindow_process.cpp ../apps/envoy/scanplanwindow_processoperations.cpp ../apps/envoy/scanplanwindow_processlifecycle.cpp ../apps/envoy/scanplanwindow_policy.cpp ../apps/envoy/scanplanwindow_policystate.cpp ../apps/envoy/scanplanwindow_creation.cpp ../apps/envoy/scanplanwindow_runtime.cpp ../apps/envoy/scanplanwindow_observation.cpp
HEADERS += ../apps/envoy/scanplanwindow.h
SOURCES += ../apps/envoy/scanplanoutput.cpp
HEADERS += ../apps/envoy/scanplanoutput.h
SOURCES += ../apps/envoy/scanplanreview.cpp ../apps/envoy/scanplanreview_record.cpp ../apps/envoy/scanplanreview_snapshot.cpp ../apps/envoy/scanplanreview_binding.cpp
HEADERS += ../apps/envoy/scanplanreview.h ../apps/envoy/scanplanreview_p.h

SOURCES += ../apps/envoy/mainwindow_console.cpp ../apps/envoy/consolepresentation.cpp ../apps/envoy/consolepresentation_icons.cpp ../apps/envoy/consolepresentation_styles.cpp
HEADERS += ../apps/envoy/consolepresentation.h

RESOURCES += ../apps/envoy/consoleicons.qrc

SOURCES += ../apps/envoy/autorunwidget.cpp ../apps/envoy/diagnostichighlighter.cpp
HEADERS += ../apps/envoy/autorunwidget.h ../apps/envoy/diagnostichighlighter.h

SOURCES += ../apps/envoy/mainwindow_tools.cpp

include(../apps/envoy/hardwaresetup.pri)

SOURCES += ../apps/envoy/mainwindow_services.cpp

SOURCES += ../apps/envoy/applicationinstance.cpp
HEADERS += ../apps/envoy/applicationinstance.h
SOURCES += ../apps/envoy/mainwindow_statemanagement.cpp
