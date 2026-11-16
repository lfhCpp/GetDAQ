# Original cconfiged UI/engine; all legacy libraries are compiled into this binary.
QT = core gui widgets
CONFIG += c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = getdaq-cconfig-editor
isEmpty(GETDAQ_PYTHON) {
    win32: GETDAQ_PYTHON = python
    else: GETDAQ_PYTHON = python3
}
ORIGINAL_BUILD = $$OUT_PWD/original
!system($$shell_quote($$GETDAQ_PYTHON) $$shell_quote($$PWD/../../tools/prepare-original-cconfig.py) --destination $$shell_quote($$ORIGINAL_BUILD)): error(Cannot prepare bundled original cconfiged sources)
include($$ORIGINAL_BUILD/original.pri)
DEFINES += BUILD_CCFG_FMT_XML=1 BUILD_CCFG_FMT_BINARY=1 TIXML_USE_STL
contains(QT_ARCH, x86_64)|contains(QT_ARCH, i386)|contains(QT_ARCH, arm64): DEFINES += WORDS_LITTLE_ENDIAN
else: error(Confirm target endianness before building original cconfiged)
include(../../common/build-output.pri)
