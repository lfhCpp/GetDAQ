SOURCES += $$PWD/localserviceprocesses.cpp
HEADERS += $$PWD/localserviceprocesses.h
linux {
 SOURCES += $$PWD/backendshutdown.cpp
 HEADERS += $$PWD/backendshutdown.h
 include($$PWD/../deployment/deploymentcore.pri)
 SOURCES += $$PWD/../deployment/deploymentcontroller.cpp $$PWD/distributeddeploymentwindow.cpp $$PWD/hostrolewindow.cpp $$PWD/existingeccconnection.cpp $$PWD/unifiedeccwindow.cpp $$PWD/deploymentpaths.cpp
 HEADERS += $$PWD/../deployment/deploymentcontroller.h $$PWD/distributeddeploymentwindow.h $$PWD/hostrolewindow.h $$PWD/existingeccconnection.h $$PWD/unifiedeccwindow.h $$PWD/deploymentpaths.h
}
