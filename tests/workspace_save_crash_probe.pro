include(xcfg_shared.pro)
TARGET = workspace-save-crash-probe
SOURCES -= test_xcfg_shared.cpp
SOURCES += workspace_save_crash_probe.cpp
CONFIG -= testcase
QT -= testlib
