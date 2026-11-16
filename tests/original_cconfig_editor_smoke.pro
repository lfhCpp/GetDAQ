include(../apps/cconfigeditor/cconfigeditor.pro)
TARGET = getdaq-original-editor-smoke
SOURCES -= $$ORIGINAL_BUILD/widgets/src/main.cpp
SOURCES += $$PWD/original_cconfig_editor_smoke.cpp
