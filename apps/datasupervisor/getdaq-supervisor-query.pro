TEMPLATE = app
QT =
CONFIG += c++11 console
CONFIG -= app_bundle
QMAKE_CXXFLAGS += -std=c++11 -pedantic-errors
TARGET = getdaq-supervisor-query
INCLUDEPATH += . ../../common/experiment ../../common/config ../../common/control
SOURCES += querymain.cpp queryrunner.cpp queryrunner_validation.cpp queryrunner_plantransition.cpp queryrunner_catalogreplacement.cpp supervisorcommon.cpp planio.cpp controlprotocol.cpp controlprotocol_request_validation.cpp controlprotocol_status.cpp
win32 {
    SOURCES += controlhttp_win.cpp credential_win.cpp queryfile_win.cpp
    QMAKE_LFLAGS += -municode
    LIBS += -lws2_32 -ladvapi32
} else {
    SOURCES += supervisorio.cpp controlhttp.cpp credential.cpp queryfile.cpp
}
include(../../common/experiment/experiment.pri)
include(../../common/control/control.pri)
SOURCES += ../../common/config/configurationrange.cpp ../../common/config/configurationrange_parse.cpp
INCLUDEPATH += ../../common/config
include(../../common/build-output.pri)
