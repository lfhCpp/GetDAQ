include(xcfg_shared.pro)
TARGET = config-transaction-probe
SOURCES -= test_xcfg_shared.cpp
SOURCES += config_transaction_probe.cpp
CONFIG -= testcase
QT -= testlib
