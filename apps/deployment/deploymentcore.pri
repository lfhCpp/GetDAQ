QT += core network concurrent
CONFIG += c++11
INCLUDEPATH += $$PWD $$PWD/../envoy $$PWD/../../common/http
SOURCES += $$PWD/deploymentcommon.cpp $$PWD/distributedproject.cpp
HEADERS += $$PWD/deploymentcommon.h $$PWD/distributedproject.h

SOURCES += $$PWD/originaldeployment.cpp
HEADERS += $$PWD/originaldeployment.h
