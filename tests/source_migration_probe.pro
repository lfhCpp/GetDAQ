include(xcfg_shared.pro)
SOURCES -= test_xcfg_shared.cpp
SOURCES += source_migration_probe.cpp
QT -= testlib
CONFIG -= testcase
TARGET = source-migration-probe
