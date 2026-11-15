INCLUDEPATH += $$PWD
SOURCES += $$PWD/xcfgworkspace.cpp \
           $$PWD/xcfgworkspace_build.cpp \
           $$PWD/xcfgworkspace_edits.cpp \
           $$PWD/xcfgworkspace_save.cpp \
           $$PWD/xcfgworkspace_structure.cpp \
           $$PWD/xcfgworkspace_move.cpp
HEADERS += $$PWD/xcfgworkspace.h
SOURCES += $$PWD/xcfgsavejournal.cpp
HEADERS += $$PWD/xcfgsavejournal.h
SOURCES += $$PWD/xcfgworkspacecopy.cpp
HEADERS += $$PWD/xcfgworkspacecopy.h
SOURCES += $$PWD/xcfgexporter.cpp
HEADERS += $$PWD/xcfgexporter.h
SOURCES += $$PWD/xcfgdocument.cpp $$PWD/xcfgdocument_io.cpp $$PWD/xcfgdocumentproperties.cpp $$PWD/xcfgscalartext.cpp
SOURCES += $$PWD/xcfgdocument_effective.cpp
HEADERS += $$PWD/xcfgdocument.h
SOURCES += $$PWD/xcfgstructureeditor.cpp \
           $$PWD/xcfgstructureeditor_apply.cpp \
           $$PWD/xcfgstructureeditor_override.cpp \
           $$PWD/xcfgstructureeditor_include.cpp \
           $$PWD/xcfgstructureeditor_materialize.cpp
HEADERS += $$PWD/xcfgstructureeditor.h $$PWD/xcfgstructureeditor_p.h
SOURCES += $$PWD/xcfgedithistory.cpp
HEADERS += $$PWD/xcfgedithistory.h
SOURCES += $$PWD/configurationrange.cpp $$PWD/configurationrange_parse.cpp
HEADERS += $$PWD/configurationrange.h $$PWD/configurationrangeinternal_p.h
SOURCES += $$PWD/effectiveconfigurationsnapshot.cpp
HEADERS += $$PWD/effectiveconfigurationsnapshot.h
SOURCES += $$PWD/systemconfigurationpublisher.cpp
HEADERS += $$PWD/systemconfigurationpublisher.h
SOURCES += $$PWD/hardwarecompiler.cpp
HEADERS += $$PWD/hardwarecompiler.h
include(../experiment/experiment.pri)
include(../hardware/hardware.pri)
SOURCES += $$PWD/agetcompiler.cpp $$PWD/agetcompiler_cobo.cpp $$PWD/agetcompiler_readout.cpp $$PWD/agetcompiler_configure.cpp
HEADERS += $$PWD/agetcompiler.h $$PWD/agetcompiler_p.h
SOURCES += $$PWD/experimentreader.cpp $$PWD/clockcompiler.cpp
HEADERS += $$PWD/experimentreader.h $$PWD/clockcompiler.h
SOURCES += $$PWD/monitoringcompiler.cpp
HEADERS += $$PWD/monitoringcompiler.h
SOURCES += $$PWD/calibrationcompiler.cpp
HEADERS += $$PWD/calibrationcompiler.h
SOURCES += $$PWD/mutantcompiler.cpp
HEADERS += $$PWD/mutantcompiler.h
SOURCES += $$PWD/systemexperimentcompiler.cpp
HEADERS += $$PWD/systemexperimentcompiler.h

SOURCES += $$PWD/xcfgbinary.cpp \
           $$PWD/xcfgbinary_codec.cpp
HEADERS += $$PWD/xcfgbinary.h $$PWD/xcfgbinary_codec_p.h

HEADERS += $$PWD/xcfgtransaction.h
SOURCES += $$PWD/xcfgsourcemigration.cpp $$PWD/xcfgsourcemigration_preview.cpp $$PWD/xcfgsourcemigration_recovery.cpp $$PWD/xcfgsourcemigration_store.cpp $$PWD/xcfgsourcemigration_store_decode.cpp $$PWD/xcfgsourcemigration_store_state.cpp $$PWD/xcfgsourcereferrers.cpp
HEADERS += $$PWD/xcfgsourcemigration.h $$PWD/xcfgsourcemigration_p.h
