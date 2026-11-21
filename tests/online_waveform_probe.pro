QT += core network
QT -= gui
CONFIG += console c++11
TEMPLATE = app
TARGET = online-waveform-probe
INCLUDEPATH += ../apps/onlinewaveform ../apps/waveformviewer
SOURCES += online_waveform_probe.cpp ../apps/onlinewaveform/onlinestore.cpp ../apps/onlinewaveform/onlinereceiver.cpp ../apps/waveformviewer/grawreader_index.cpp ../apps/waveformviewer/grawreader_waveform.cpp ../apps/waveformviewer/grawreader_decode_type1.cpp ../apps/waveformviewer/grawreader_decode_type2.cpp
HEADERS += ../apps/onlinewaveform/onlinereceiver.h
