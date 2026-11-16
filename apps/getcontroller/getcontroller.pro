# Original GetController application, isolated from GetDAQ's acquisition process.
QT = core gui widgets network xml printsupport opengl concurrent
CONFIG += c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = getdaq-getcontroller
isEmpty(GETDAQ_ICE_BUILD): error(Set GETDAQ_ICE_BUILD to the offline Ice compatibility build directory)
isEmpty(GETDAQ_PYTHON): GETDAQ_PYTHON = python3
ORIGINAL_BUILD = $$OUT_PWD/original
!system($$shell_quote($$GETDAQ_PYTHON) $$shell_quote($$PWD/../../tools/prepare-original-getcontroller.py) --destination $$shell_quote($$ORIGINAL_BUILD) --ice-build $$shell_quote($$GETDAQ_ICE_BUILD)): error(Cannot prepare original GetController)
include($$ORIGINAL_BUILD/getcontroller.pri)
DEFINES += BUILD_CCFG_FMT_XML=1 BUILD_CCFG_FMT_BINARY=1 TIXML_USE_STL WORDS_LITTLE_ENDIAN HAVE_ICESTORM=1
isEmpty(GETDAQ_SVG_HEADERS) {
    QT += svg
} else {
    INCLUDEPATH += $$GETDAQ_SVG_HEADERS $$GETDAQ_SVG_HEADERS/..
    LIBS += -l:libQt5Svg.so.5
    DEFINES += QT_SVG_LIB
}
include(../../common/build-output.pri)
QMAKE_CXXFLAGS += -include $$shell_quote($$PWD/buildcompat.h)
