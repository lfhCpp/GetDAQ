cmake_minimum_required(VERSION 3.12)
project(GetDAQReceiver VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT UNIX)
  message(FATAL_ERROR "The native receiver currently targets Linux/macOS. The Qt GUI has a separate Windows build.")
endif()
find_package(Threads REQUIRED)
include(CTest)
option(GETDAQ_BUILD_OPTIONAL_FITS_TOOLS "Build the offline FITS analysis converter (requires system CFITSIO)" OFF)
option(GETDAQ_BUILD_OPTIONAL_ROOT_TOOLS "Build the offline legacy ROOT converter (requires ROOT 6)" OFF)
option(GETDAQ_BUILD_OPTIONAL_ROOT_VIEWER "Build the offline Qt ROOT event browser (requires ROOT 6 and Qt 5)" OFF)
if(CMAKE_SIZEOF_VOID_P LESS 8)
  message(FATAL_ERROR "The high-volume receiver requires a 64-bit build.")
endif()
add_library(getdaq-data-core mfmstream.cpp diskreserve.cpp framestore.cpp rawfile.cpp hitpattern.cpp)
target_include_directories(getdaq-data-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(getdaq-data-core PRIVATE -Wall -Wextra -Wpedantic)
add_executable(getdaq-inspect inspect.cpp inspect_hitpattern.cpp)
target_link_libraries(getdaq-inspect PRIVATE getdaq-data-core)
target_compile_options(getdaq-inspect PRIVATE -Wall -Wextra -Wpedantic)
add_executable(getdaq-merge merge_main.cpp merge.cpp merge_index.cpp merge_output.cpp)
target_link_libraries(getdaq-merge PRIVATE getdaq-data-core)
target_compile_options(getdaq-merge PRIVATE -Wall -Wextra -Wpedantic)
add_executable(getdaq-matrix matrix_main.cpp matrix.cpp matrix_codec.cpp)
target_link_libraries(getdaq-matrix PRIVATE getdaq-data-core)
target_compile_options(getdaq-matrix PRIVATE -Wall -Wextra -Wpedantic)
if(GETDAQ_BUILD_OPTIONAL_FITS_TOOLS)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(CFITSIO REQUIRED IMPORTED_TARGET cfitsio)
  add_executable(getdaq-fits fits_main.cpp fits.cpp fits_decode.cpp fits_writer.cpp fits_writer_event.cpp fits_writer_support.cpp)
  target_link_libraries(getdaq-fits PRIVATE getdaq-data-core PkgConfig::CFITSIO)
  target_compile_options(getdaq-fits PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(GETDAQ_BUILD_OPTIONAL_ROOT_TOOLS OR GETDAQ_BUILD_OPTIONAL_ROOT_VIEWER)
  find_package(ROOT 6 CONFIG REQUIRED COMPONENTS Core RIO Tree)
  if(NOT COMMAND ROOT_GENERATE_DICTIONARY)
    include(${ROOT_DIR}/RootMacros.cmake)
  endif()
  set(GETDAQ_ROOT_MODEL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/coboframe-root-model)
  set(GETDAQ_ROOT_MODEL_HEADERS
    ${GETDAQ_ROOT_MODEL_DIR}/GDataFrame.h
    ${GETDAQ_ROOT_MODEL_DIR}/GDataChannel.h
    ${GETDAQ_ROOT_MODEL_DIR}/GDataSample.h
    ${GETDAQ_ROOT_MODEL_DIR}/GFrameHeader.h
    ${GETDAQ_ROOT_MODEL_DIR}/T2KEvent.h)
  ROOT_GENERATE_DICTIONARY(G__GetDAQRootModel ${GETDAQ_ROOT_MODEL_HEADERS}
    LINKDEF ${GETDAQ_ROOT_MODEL_DIR}/LinkDefGetDAQ.h
    OPTIONS -I${GETDAQ_ROOT_MODEL_DIR})
  if(GETDAQ_BUILD_OPTIONAL_ROOT_TOOLS)
    add_executable(getdaq-root root_main.cpp rootconverter.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataFrame.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataChannel.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataSample.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GFrameHeader.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/T2KEvent.cpp
      ${CMAKE_CURRENT_BINARY_DIR}/G__GetDAQRootModel.cxx)
    target_include_directories(getdaq-root PRIVATE ${GETDAQ_ROOT_MODEL_DIR})
    target_link_libraries(getdaq-root PRIVATE getdaq-data-core ROOT::Core ROOT::RIO ROOT::Tree)
    set_property(TARGET getdaq-root PROPERTY CXX_STANDARD 17)
    target_compile_options(getdaq-root PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  if(GETDAQ_BUILD_OPTIONAL_ROOT_VIEWER)
    find_package(Qt5 5.12 REQUIRED COMPONENTS Core Widgets)
    ROOT_GENERATE_DICTIONARY(G__GetDAQRoot5Model ${GETDAQ_ROOT_MODEL_HEADERS}
      LINKDEF ${GETDAQ_ROOT_MODEL_DIR}/LinkDefGetDAQRoot5.h
      OPTIONS -DGETDAQ_FORCE_ROOT5_NAMESPACE -I${GETDAQ_ROOT_MODEL_DIR})
    add_library(getdaq-root5-model OBJECT
      ${GETDAQ_ROOT_MODEL_DIR}/GDataFrame.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataChannel.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataSample.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GFrameHeader.cpp
      ${CMAKE_CURRENT_BINARY_DIR}/G__GetDAQRoot5Model.cxx)
    target_include_directories(getdaq-root5-model PRIVATE ${GETDAQ_ROOT_MODEL_DIR})
    target_compile_definitions(getdaq-root5-model PRIVATE GETDAQ_FORCE_ROOT5_NAMESPACE)
    target_link_libraries(getdaq-root5-model PRIVATE ROOT::Core ROOT::RIO ROOT::Tree)
    set_property(TARGET getdaq-root5-model PROPERTY CXX_STANDARD 17)
    set_source_files_properties(../../apps/rootviewer/root5treeadapter.cpp PROPERTIES
      COMPILE_DEFINITIONS GETDAQ_FORCE_ROOT5_NAMESPACE)
    add_executable(getdaq-root-viewer
      ../../apps/rootviewer/main.cpp
      ../../apps/rootviewer/rootviewerwindow.cpp
      ../../apps/rootviewer/rootviewerwindow_navigation.cpp
      ../../apps/rootviewer/rootviewerwindow_statistics.cpp
      ../../apps/rootviewer/rootcanvas.cpp
      ../../apps/rootviewer/rootcanvas_eventplots.cpp
      ../../apps/rootviewer/rootcanvas_surface.cpp
      ../../apps/rootviewer/roottreereader.cpp
      ../../apps/rootviewer/root5treeadapter.cpp
      ../../apps/rootviewer/t2keventadapter.cpp
      ../../apps/rootviewer/rootstatistics.cpp
      ../../apps/rootviewer/rootstatsscanner.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataFrame.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataChannel.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataSample.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GFrameHeader.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/T2KEvent.cpp
      ${CMAKE_CURRENT_BINARY_DIR}/G__GetDAQRootModel.cxx)
    target_sources(getdaq-root-viewer PRIVATE $<TARGET_OBJECTS:getdaq-root5-model>)
    target_include_directories(getdaq-root-viewer PRIVATE ${GETDAQ_ROOT_MODEL_DIR} ../../apps/rootviewer)
    target_link_libraries(getdaq-root-viewer PRIVATE ROOT::Core ROOT::RIO ROOT::Tree Qt5::Core Qt5::Widgets)
    set_property(TARGET getdaq-root-viewer PROPERTY CXX_STANDARD 17)
    target_compile_options(getdaq-root-viewer PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()
