QT += core network
QT -= gui
CONFIG += console c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = getdaq-control-mcp

SOURCES += main.cpp mcpserver_protocol.cpp mcpserver_transport.cpp mcpserver_readtools.cpp mcpserver_controltools.cpp mcpsettings.cpp mcptoolschema.cpp mcpstdio.cpp
HEADERS += mcpserver_p.h mcpsettings.h mcptoolschema.h mcpstdio.h
include(../../../common/build-output.pri)
