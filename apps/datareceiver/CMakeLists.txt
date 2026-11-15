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
add_executable(getdaq-receiver main.cpp receiver_options.cpp receiver_identity.cpp receiver.cpp receiver_write.cpp receiver_write_bytestorage.cpp receiver_write_framestorage.cpp receiver_seal.cpp receiver_status.cpp exporter.cpp bytestore.cpp)
target_link_libraries(getdaq-receiver PRIVATE getdaq-data-core Threads::Threads)
target_compile_options(getdaq-receiver PRIVATE -Wall -Wextra -Wpedantic)
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
if(BUILD_TESTING)
  add_executable(receiver-options-tests ../../tests/receiver_options_test.cpp receiver_options.cpp)
  target_include_directories(receiver-options-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME receiver-options COMMAND receiver-options-tests)
  add_executable(merge-tests ../../tests/merge_test.cpp merge.cpp merge_index.cpp merge_output.cpp)
  target_link_libraries(merge-tests PRIVATE getdaq-data-core)
  add_test(NAME mfm-merge COMMAND merge-tests)
  add_executable(matrix-tests ../../tests/matrix_test.cpp matrix.cpp matrix_codec.cpp)
  target_link_libraries(matrix-tests PRIVATE getdaq-data-core)
  add_test(NAME gmat-conversion COMMAND matrix-tests)
  add_executable(hitpattern-tests ../../tests/hitpattern_test.cpp)
  target_link_libraries(hitpattern-tests PRIVATE getdaq-data-core)
  target_compile_options(hitpattern-tests PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME cobo-hit-pattern COMMAND hitpattern-tests)
  if(GETDAQ_BUILD_OPTIONAL_FITS_TOOLS)
    add_executable(fits-tests ../../tests/fits_test.cpp fits.cpp fits_decode.cpp fits_writer.cpp fits_writer_event.cpp fits_writer_support.cpp)
    target_link_libraries(fits-tests PRIVATE getdaq-data-core PkgConfig::CFITSIO)
    target_compile_options(fits-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME fits-conversion COMMAND fits-tests)
  endif()
  if(GETDAQ_BUILD_OPTIONAL_ROOT_TOOLS)
    add_executable(root-tests ../../tests/root_conversion_test.cpp rootconverter.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataFrame.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataChannel.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataSample.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GFrameHeader.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/T2KEvent.cpp
      ${CMAKE_CURRENT_BINARY_DIR}/G__GetDAQRootModel.cxx)
    target_include_directories(root-tests PRIVATE ${GETDAQ_ROOT_MODEL_DIR})
    target_link_libraries(root-tests PRIVATE getdaq-data-core ROOT::Core ROOT::RIO ROOT::Tree)
    set_property(TARGET root-tests PROPERTY CXX_STANDARD 17)
    target_compile_options(root-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME root-conversion COMMAND root-tests)
  endif()
  if(GETDAQ_BUILD_OPTIONAL_ROOT_VIEWER)
    add_executable(root-viewer-tests ../../tests/root_viewer_reader_test.cpp
      ../../tests/root5_tree_fixture.cpp
      ../../apps/rootviewer/roottreereader.cpp
      ../../apps/rootviewer/root5treeadapter.cpp
      ../../apps/rootviewer/t2keventadapter.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataFrame.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataChannel.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataSample.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GFrameHeader.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/T2KEvent.cpp
      ${CMAKE_CURRENT_BINARY_DIR}/G__GetDAQRootModel.cxx)
    target_sources(root-viewer-tests PRIVATE $<TARGET_OBJECTS:getdaq-root5-model>)
    set_source_files_properties(../../tests/root5_tree_fixture.cpp PROPERTIES
      COMPILE_DEFINITIONS GETDAQ_FORCE_ROOT5_NAMESPACE)
    target_include_directories(root-viewer-tests PRIVATE ${GETDAQ_ROOT_MODEL_DIR} ../../apps/rootviewer ../../tests)
    target_link_libraries(root-viewer-tests PRIVATE ROOT::Core ROOT::RIO ROOT::Tree Qt5::Core)
    set_property(TARGET root-viewer-tests PROPERTY CXX_STANDARD 17)
    target_compile_options(root-viewer-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME root-viewer-reader COMMAND root-viewer-tests)
    add_executable(root-statistics-tests ../../tests/root_statistics_test.cpp
      ../../apps/rootviewer/rootstatistics.cpp)
    target_include_directories(root-statistics-tests PRIVATE ../../apps/rootviewer)
    set_property(TARGET root-statistics-tests PROPERTY CXX_STANDARD 11)
    target_compile_options(root-statistics-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME root-statistics COMMAND root-statistics-tests)
    add_executable(root-statistics-scan-tests ../../tests/root_statistics_scan_test.cpp
      ../../apps/rootviewer/rootstatistics.cpp ../../apps/rootviewer/rootstatsscanner.cpp
      ../../apps/rootviewer/roottreereader.cpp
      ../../apps/rootviewer/root5treeadapter.cpp
      ../../apps/rootviewer/t2keventadapter.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataFrame.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataChannel.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GDataSample.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/GFrameHeader.cpp
      ${GETDAQ_ROOT_MODEL_DIR}/T2KEvent.cpp
      ${CMAKE_CURRENT_BINARY_DIR}/G__GetDAQRootModel.cxx)
    target_sources(root-statistics-scan-tests PRIVATE $<TARGET_OBJECTS:getdaq-root5-model>)
    target_include_directories(root-statistics-scan-tests PRIVATE
      ${GETDAQ_ROOT_MODEL_DIR} ../../apps/rootviewer)
    target_link_libraries(root-statistics-scan-tests PRIVATE ROOT::Core ROOT::RIO ROOT::Tree Threads::Threads)
    set_property(TARGET root-statistics-scan-tests PROPERTY CXX_STANDARD 17)
    target_compile_options(root-statistics-scan-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME root-statistics-scan COMMAND root-statistics-scan-tests)
  endif()
  add_executable(receiver-core-tests ../../tests/receiver_core_test.cpp bytestore.cpp)
  target_link_libraries(receiver-core-tests PRIVATE getdaq-data-core Threads::Threads)
  add_test(NAME receiver-core COMMAND receiver-core-tests)
  set_tests_properties(receiver-core PROPERTIES TIMEOUT 30)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(exporter-lock-tests ../../tests/exporter_lock_test.cpp exporter.cpp)
    target_include_directories(exporter-lock-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(exporter-lock-tests PRIVATE Threads::Threads)
    set_property(TARGET exporter-lock-tests APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--wrap=send -Wl,--wrap=accept")
    add_test(NAME exporter-lock COMMAND exporter-lock-tests)
    set_tests_properties(exporter-lock PROPERTIES TIMEOUT 15)
  endif()
  add_executable(mfm-stream-benchmark ../../tests/mfm_stream_benchmark.cpp)
  target_link_libraries(mfm-stream-benchmark PRIVATE getdaq-data-core)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME hitpattern-cli
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/run_hitpattern_scan_package_test.py
              --inspect-binary $<TARGET_FILE:getdaq-inspect>
              --result ${CMAKE_CURRENT_BINARY_DIR}/hitpattern-cli-test-receipt.json)
    add_test(NAME byte-storage-integration
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/byte_storage_integration.py $<TARGET_FILE:getdaq-receiver>)
    set_tests_properties(byte-storage-integration PROPERTIES TIMEOUT 30)
  endif()
endif()
