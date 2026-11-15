include(tests.pro)
SOURCES -= test_getdaq.cpp runrecorder_tests.cpp sentry_service_tests.cpp soap_protocol_tests.cpp operation_log_tests.cpp native_status_tests.cpp scanplan_output_tests.cpp native_gui_tests.cpp
SOURCES -= waveformviewer_tests.cpp waveformviewer_testfixtures.cpp autorun_tests.cpp consolelayout_tests.cpp offline_tools_tests.cpp native_recovery_policy_tests.cpp configuration_compiler_tests.cpp mocknode_fixture.cpp legacy_runcontrol_tests.cpp getdaq_test_support.cpp native_scan_run_tests.cpp scanplan_builder_gui_tests.cpp
SOURCES += cobo_maintenance_window_probe.cpp
TARGET = cobo-maintenance-window-probe
CONFIG -= testcase
QT -= testlib
