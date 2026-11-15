set(SOURCE_CONTROL_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${SOURCE_CONTROL_GENERATED}")
add_custom_command(OUTPUT "${SOURCE_CONTROL_GENERATED}/SourceControl.cpp" "${SOURCE_CONTROL_GENERATED}/SourceControl.h"
  COMMAND slice2cpp "-I${MDAQ}" "-I${GET}" "-I${ICE}/slice" --output-dir "${SOURCE_CONTROL_GENERATED}" "${CMAKE_CURRENT_SOURCE_DIR}/../SourceControl.ice"
  DEPENDS slice2cpp compat-get-headers "${CMAKE_CURRENT_SOURCE_DIR}/../SourceControl.ice" "${MDAQ}/mdaq/DaqControl.ice" VERBATIM)
add_library(getdaq-source-control-protocol STATIC "${SOURCE_CONTROL_GENERATED}/SourceControl.cpp")
target_include_directories(getdaq-source-control-protocol PUBLIC "${SOURCE_CONTROL_GENERATED}")
target_link_libraries(getdaq-source-control-protocol PUBLIC compat-GET)
set_target_properties(getdaq-source-control-protocol PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(getdaq-source-control-protocol PRIVATE -Wno-deprecated-declarations)
