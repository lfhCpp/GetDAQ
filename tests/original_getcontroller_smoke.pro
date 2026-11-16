include(../apps/getcontroller/getcontroller.pro)
TARGET = getdaq-original-getcontroller-smoke
SOURCES -= $$GETDAQ_CONTROLLER_MAIN
SOURCES += $$PWD/original_getcontroller_smoke.cpp
