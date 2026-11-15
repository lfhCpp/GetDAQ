SOURCES += $$PWD/hardwaredeployment.cpp $$PWD/hardwaresetupwindow.cpp
HEADERS += $$PWD/hardwaredeployment.h $$PWD/hardwaresetupwindow.h

SOURCES += $$PWD/localserviceswindow.cpp
HEADERS += $$PWD/localserviceswindow.h

SOURCES += $$PWD/coboconnectionwindow.cpp
HEADERS += $$PWD/coboconnectionwindow.h

SOURCES += $$PWD/localserviceprocesses.cpp
HEADERS += $$PWD/localserviceprocesses.h

SOURCES += $$PWD/localdeploymentservices.cpp
HEADERS += $$PWD/localdeploymentservices.h

SOURCES += $$PWD/earlywarningrecovery.cpp
HEADERS += $$PWD/earlywarningrecovery.h
SOURCES += $$PWD/backendpresets.cpp
HEADERS += $$PWD/backendpresets.h
RESOURCES += $$PWD/backendpresets.qrc
SOURCES += $$PWD/localbackendregistry.cpp
HEADERS += $$PWD/localbackendregistry.h
linux {
    include($$PWD/../deployment/deploymentcore.pri)
    SOURCES += $$PWD/../deployment/deploymentcontroller.cpp $$PWD/distributeddeploymentwindow.cpp $$PWD/hostrolewindow.cpp $$PWD/existingeccconnection.cpp $$PWD/unifiedeccwindow.cpp
    HEADERS += $$PWD/../deployment/deploymentcontroller.h $$PWD/distributeddeploymentwindow.h $$PWD/hostrolewindow.h $$PWD/existingeccconnection.h $$PWD/unifiedeccwindow.h
}
