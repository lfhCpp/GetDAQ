QT += core testlib
QT -= gui
CONFIG += c++11 testcase console
TEMPLATE = app
TARGET = xcfg-shared-tests
SOURCES += ../common/config/xcfgworkspace.cpp \
           ../common/config/xcfgworkspace_build.cpp \
           ../common/config/xcfgworkspace_edits.cpp \
           ../common/config/xcfgworkspace_save.cpp \
           ../common/config/xcfgworkspace_structure.cpp \
           ../common/config/xcfgworkspace_move.cpp
HEADERS += ../common/config/xcfgworkspace.h
SOURCES += ../common/config/xcfgsavejournal.cpp
HEADERS += ../common/config/xcfgsavejournal.h
INCLUDEPATH += ../common/config
SOURCES += test_xcfg_shared.cpp ../common/config/xcfgdocument.cpp ../common/config/xcfgdocument_io.cpp ../common/config/xcfgdocumentproperties.cpp ../common/config/xcfgscalartext.cpp ../common/config/xcfgdocument_effective.cpp ../common/config/experimentreader.cpp ../common/config/configurationrange.cpp ../common/config/configurationrange_parse.cpp
HEADERS += ../common/config/xcfgdocument.h ../common/config/experimentreader.h ../common/config/configurationrange.h ../common/config/configurationrangeinternal_p.h
SOURCES += ../common/config/xcfgstructureeditor.cpp ../common/config/xcfgstructureeditor_apply.cpp ../common/config/xcfgstructureeditor_override.cpp ../common/config/xcfgstructureeditor_include.cpp ../common/config/xcfgstructureeditor_materialize.cpp
HEADERS += ../common/config/xcfgstructureeditor.h ../common/config/xcfgstructureeditor_p.h
SOURCES += ../common/config/xcfgedithistory.cpp
HEADERS += ../common/config/xcfgedithistory.h
SOURCES += ../common/config/xcfgexporter.cpp
HEADERS += ../common/config/xcfgexporter.h
SOURCES += ../common/config/xcfgworkspacecopy.cpp
HEADERS += ../common/config/xcfgworkspacecopy.h
INCLUDEPATH += ../common/experiment
SOURCES += ../common/config/effectiveconfigurationsnapshot.cpp ../common/experiment/strictjson.cpp
HEADERS += ../common/config/effectiveconfigurationsnapshot.h ../common/experiment/strictjson.h
SOURCES += ../common/experiment/effectiveconfigurationrecord.cpp ../common/experiment/configurationsha256.cpp
HEADERS += ../common/experiment/effectiveconfigurationrecord.h ../common/experiment/configurationsha256.h
strict_c++: QMAKE_CXXFLAGS += -pedantic-errors

SOURCES += ../common/config/xcfgbinary.cpp ../common/config/xcfgbinary_codec.cpp
HEADERS += ../common/config/xcfgbinary.h ../common/config/xcfgbinary_codec_p.h

HEADERS += ../common/config/xcfgtransaction.h
SOURCES += ../common/config/xcfgsourcemigration.cpp ../common/config/xcfgsourcemigration_preview.cpp ../common/config/xcfgsourcemigration_recovery.cpp ../common/config/xcfgsourcemigration_store.cpp ../common/config/xcfgsourcemigration_store_decode.cpp ../common/config/xcfgsourcemigration_store_state.cpp ../common/config/xcfgsourcereferrers.cpp
HEADERS += ../common/config/xcfgsourcemigration.h ../common/config/xcfgsourcemigration_p.h
