INCLUDEPATH += $$PWD
SOURCES += $$PWD/hardwarestateprotocol.cpp
HEADERS += $$PWD/hardwarestateprotocol.h
SOURCES += $$PWD/eccprotocol.cpp $$PWD/eccprotocol_request.cpp $$PWD/eccprotocol_request_validation.cpp $$PWD/eccprotocol_configuration.cpp $$PWD/eccprotocol_observation.cpp
SOURCES += $$PWD/eccplantransitionprotocol.cpp
HEADERS += $$PWD/eccprotocol.h $$PWD/eccprotocolinternal_p.h
SOURCES += $$PWD/transferprotocol.cpp
HEADERS += $$PWD/transferprotocol.h

SOURCES += $$PWD/coldhandoffprotocol.cpp
HEADERS += $$PWD/coldhandoffprotocol.h
SOURCES += $$PWD/scanruntimeidentity.cpp
SOURCES += $$PWD/scanruntimeidentity_initial.cpp
SOURCES += $$PWD/scanruntimeidentity_codec.cpp
SOURCES += $$PWD/scanruntimeidentity_deployment.cpp
HEADERS += $$PWD/scanruntimeidentity.h $$PWD/scanruntimeidentityinternal_p.h
SOURCES += $$PWD/scaneventcountobservation.cpp
HEADERS += $$PWD/scaneventcountobservation.h
