QT += core gui widgets
CONFIG += c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = getdaq-waveform-viewer
SOURCES += main.cpp waveformviewer_cli.cpp
HEADERS += waveformviewer_cli.h
include(waveformwindow.pri)
include(../../common/build-output.pri)
win32-msvc*: QMAKE_CXXFLAGS += /utf-8
