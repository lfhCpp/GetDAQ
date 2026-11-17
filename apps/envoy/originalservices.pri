SOURCES += $$PWD/localserviceprocesses.cpp
HEADERS += $$PWD/localserviceprocesses.h
linux {
 include($$PWD/../deployment/deploymentcore.pri)
 SOURCES += $$PWD/../deployment/deploymentcontroller.cpp $$PWD/distributeddeploymentwindow.cpp $$PWD/hostrolewindow.cpp $$PWD/existingeccconnection.cpp $$PWD/unifiedeccwindow.cpp
 HEADERS += $$PWD/../deployment/deploymentcontroller.h $$PWD/distributeddeploymentwindow.h $$PWD/hostrolewindow.h $$PWD/existingeccconnection.h $$PWD/unifiedeccwindow.h
}
