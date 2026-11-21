QT += widgets network concurrent
CONFIG += c++11
TEMPLATE = app
TARGET = getdaq-online-waveform-viewer
INCLUDEPATH += ../waveformviewer
SOURCES += main.cpp onlinestore.cpp onlinereceiver.cpp ../waveformviewer/grawreader_index.cpp ../waveformviewer/grawreader_waveform.cpp ../waveformviewer/grawreader_decode_type1.cpp ../waveformviewer/grawreader_decode_type2.cpp ../waveformviewer/waveformcanvas.cpp ../waveformviewer/waveformcanvas_surface.cpp ../waveformviewer/waveformexport.cpp
HEADERS += onlinestore.h onlinereceiver.h
!isEmpty(GETDAQ_DESTDIR): DESTDIR = $$GETDAQ_DESTDIR
