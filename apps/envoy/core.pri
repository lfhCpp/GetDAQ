QT += core network concurrent
CONFIG += c++11
include($$PWD/../../common/config/xcfg.pri)
include($$PWD/../../common/control/control.pri)
INCLUDEPATH += $$PWD $$PWD/../../third_party/yaml-cpp/include $$PWD/../../third_party/yaml-cpp/src
DEFINES += YAML_CPP_STATIC_DEFINE
SOURCES += $$PWD/runconfig.cpp $$PWD/runconfig_yaml.cpp $$PWD/protocol.cpp $$PWD/httpclient.cpp \
           $$PWD/eccclient.cpp $$PWD/eccsystemclient.cpp $$PWD/eccsystemclient_operations.cpp $$PWD/eccsystemclient_observations.cpp $$PWD/eccsystemclient_control.cpp $$PWD/eccsystemclient_maintenance.cpp $$PWD/sentryclient.cpp $$PWD/runcontrol.cpp $$PWD/runcontrol_observation.cpp $$PWD/runcontrol_commands.cpp $$PWD/runcontrol_execution.cpp $$PWD/runcontrol_native.cpp $$PWD/runcontrol_finalization.cpp $$PWD/runrecorder.cpp $$PWD/runrecorder_snapshot.cpp $$PWD/runrecorder_replacement.cpp
SOURCES += $$PWD/eccclientstore.cpp $$PWD/eccclientstore_state.cpp $$PWD/eccclientstore_queries.cpp \
           $$PWD/eccclientstore_retirement.cpp $$PWD/eccclientstore_mutations.cpp \
           $$PWD/eccclientstore_observationpolicy.cpp $$PWD/eccclientstore_recoverypolicy.cpp
SOURCES += $$PWD/nativeruncontrol.cpp $$PWD/nativeruncontrol_connection.cpp $$PWD/nativeruncontrol_observation.cpp
SOURCES += $$PWD/nativeruncontrol_scan.cpp
SOURCES += $$PWD/nativeruncontrol_scanrecovery.cpp
SOURCES += $$PWD/nativeruncontrol_scanidentity.cpp $$PWD/nativeruncontrol_scanbaseline.cpp
SOURCES += $$PWD/nativeruncontrol_scanfinalization.cpp
SOURCES += $$PWD/nativeruncontrol_recovery.cpp
SOURCES += $$PWD/nativeruncontrol_maintenance.cpp
SOURCES += $$PWD/nativeruncontrol_lifecycle.cpp
SOURCES += $$PWD/nativeruncontrol_plantransition.cpp
SOURCES += $$PWD/nativestatusprojection.cpp
SOURCES += $$PWD/nativestatusobservations.cpp
SOURCES += $$PWD/nativereceiverfinalizationpolicy.cpp
SOURCES += $$PWD/nativerecoverysnapshotpolicy.cpp
SOURCES += $$PWD/nativerecoveryevidencepolicy.cpp
SOURCES += $$PWD/nativecatalogrecoverypolicy.cpp
SOURCES += $$PWD/nativealarmrecoverypolicy.cpp
SOURCES += $$PWD/nativecobomaintenancepolicy.cpp
SOURCES += $$PWD/nativeplanretirementprojection.cpp
HEADERS += $$PWD/runconfig.h $$PWD/protocol.h $$PWD/httpclient.h \
           $$PWD/eccclient.h $$PWD/eccsystemclient.h $$PWD/eccsystemclient_p.h $$PWD/sentryclient.h $$PWD/runcontrol.h $$PWD/runrecorder.h $$PWD/runrecorder_p.h
HEADERS += $$PWD/configvaluedelegate_p.h
HEADERS += $$PWD/eccclientstore.h $$PWD/eccclientstore_impl_p.h \
           $$PWD/eccclientstore_observationpolicy.h $$PWD/eccclientstore_recoverypolicy.h
HEADERS += $$PWD/nativeruncontrol.h $$PWD/nativestatusprojection.h $$PWD/nativestatusobservations.h $$PWD/nativereceiverfinalizationpolicy.h $$PWD/nativerecoverysnapshotpolicy.h $$PWD/nativerecoveryevidencepolicy.h $$PWD/nativecatalogrecoverypolicy.h $$PWD/nativealarmrecoverypolicy.h $$PWD/nativecobomaintenancepolicy.h $$PWD/nativeplanretirementprojection.h
SOURCES += $$PWD/configurationrepositoryclient.cpp
HEADERS += $$PWD/configurationrepositoryclient.h
SOURCES += $$files($$PWD/../../third_party/yaml-cpp/src/*.cpp) $$files($$PWD/../../third_party/yaml-cpp/src/contrib/*.cpp)

INCLUDEPATH += $$PWD/../ecc
HEADERS += $$PWD/../ecc/scanexecutionjournalstore.h
HEADERS += $$PWD/../ecc/scanruntimeidentitybinding.h
SOURCES += $$PWD/../ecc/scanruntimeidentitybinding.cpp
HEADERS += $$PWD/../ecc/scanruntimeidentitycoordinator.h
SOURCES += $$PWD/../ecc/scanruntimeidentitycoordinator.cpp
HEADERS += $$PWD/../ecc/scanoperationrunner.h
SOURCES += $$PWD/../ecc/scanoperationrunner.cpp
HEADERS += $$PWD/../ecc/scaneventcountbaselinecoordinator.h
SOURCES += $$PWD/../ecc/scaneventcountbaselinecoordinator.cpp
win32 {
    HEADERS += $$PWD/../ecc/scanexecutionjournalstore_windows_support_p.h \
               $$PWD/../ecc/scanexecutionjournalstore_windows_io_p.h
    SOURCES += $$PWD/../ecc/scanexecutionjournalstore_windows.cpp \
               $$PWD/../ecc/scanexecutionjournalstore_windows_io.cpp \
               $$PWD/../ecc/scanexecutionjournalstore_windows_support.cpp
    LIBS += -ladvapi32
} else {
    SOURCES += $$PWD/../ecc/scanexecutionjournalstore.cpp \
               $$PWD/../ecc/scanexecutionjournalstore_read.cpp \
               $$PWD/../ecc/scanexecutionjournalstore_publish.cpp
}

SOURCES += $$PWD/autorun.cpp $$PWD/diagnostics.cpp $$PWD/runcontrol_autorun.cpp $$PWD/nativeruncontrol_autorun.cpp
HEADERS += $$PWD/autorun.h $$PWD/diagnostics.h
SOURCES += $$PWD/nativeruncontrol_inspection.cpp
SOURCES += $$PWD/nativeruncontrol_recoveryactions.cpp
SOURCES += $$PWD/configurationrepositoryclient_upload.cpp
HEADERS += $$PWD/configurationrepositoryclient_p.h
SOURCES += $$PWD/cobomaintenancewindow_actions.cpp
SOURCES += $$PWD/nativestatusprojection_failure.cpp
HEADERS += $$PWD/nativestatusprojection_p.h
SOURCES += $$PWD/eccclientstore_storage.cpp
HEADERS += $$PWD/eccclientstore_storage_p.h
SOURCES += $$PWD/nativeruncontrol_scanconfirmation.cpp
SOURCES += $$PWD/nativeruncontrol_scanlifecycleconfirmation.cpp
SOURCES += $$PWD/nativeruncontrol_scanrunconfirmation.cpp
