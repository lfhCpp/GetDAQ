# Optional legacy ICE input; the default native TCP receiver does not link Ice.
set(ICE_INGRESS_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${ICE_INGRESS_GENERATED}")
add_custom_command(OUTPUT "${ICE_INGRESS_GENERATED}/IceIngress.cpp" "${ICE_INGRESS_GENERATED}/IceIngress.h"
    COMMAND slice2cpp "-I${MDAQ}" "-I${GET}" "-I${ICE}/slice"
        --output-dir "${ICE_INGRESS_GENERATED}" "${CMAKE_CURRENT_SOURCE_DIR}/IceIngress.ice"
    DEPENDS slice2cpp compat-get-headers "${CMAKE_CURRENT_SOURCE_DIR}/IceIngress.ice"
        "${MDAQ}/mdaq/daq/DataProcessor.ice" VERBATIM)
add_library(getdaq-ice-ingress-core STATIC iceingress.cpp iceingress_preflight.cpp iceingress_status.cpp iceingress_writer_bytes.cpp iceingress_writer_frames.cpp iceingress_receipt.cpp ../datareceiver/mfmstream.cpp ../datareceiver/diskreserve.cpp ../datareceiver/framestore.cpp ../datareceiver/rawfile.cpp ../datareceiver/bytestore.cpp ../datareceiver/exporter.cpp)
target_include_directories(getdaq-ice-ingress-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../datareceiver")
target_link_libraries(getdaq-ice-ingress-core PUBLIC Threads::Threads)
set_target_properties(getdaq-ice-ingress-core PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(getdaq-ice-ingress-core PRIVATE -Wall -Wextra -pedantic-errors)
add_library(getdaq-ice-ingress-protocol STATIC "${ICE_INGRESS_GENERATED}/IceIngress.cpp")
target_include_directories(getdaq-ice-ingress-protocol PUBLIC "${ICE_INGRESS_GENERATED}")
target_link_libraries(getdaq-ice-ingress-protocol PUBLIC compat-GET)
add_library(getdaq-ice-source STATIC icesourceclient.cpp)
target_include_directories(getdaq-ice-source PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(getdaq-ice-source PUBLIC getdaq-ice-ingress-protocol)
set_target_properties(getdaq-ice-source PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(getdaq-ice-source PRIVATE -Wall -Wextra -Wno-deprecated-declarations -pedantic-errors)
add_executable(getdaq-ice-send sendmain.cpp)
target_link_libraries(getdaq-ice-send PRIVATE getdaq-ice-source)
set_target_properties(getdaq-ice-send PROPERTIES CXX_EXTENSIONS OFF RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_compile_options(getdaq-ice-send PRIVATE -Wall -Wextra -Wno-deprecated-declarations -pedantic-errors)
add_executable(getdaq-ice-receiver main.cpp iceingress_options.cpp)
target_link_libraries(getdaq-ice-receiver PRIVATE getdaq-ice-ingress-core getdaq-ice-ingress-protocol)
set_target_properties(getdaq-ice-receiver PROPERTIES CXX_EXTENSIONS OFF RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_compile_options(getdaq-ice-receiver PRIVATE -Wall -Wextra -Wno-deprecated-declarations -pedantic-errors)
if(BUILD_TESTING)
    add_executable(ice-ingress-options-tests
        "${REPOSITORY}/tests/ice_ingress_options_test.cpp"
        iceingress_options.cpp)
    target_include_directories(ice-ingress-options-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    set_target_properties(ice-ingress-options-tests PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_options(ice-ingress-options-tests PRIVATE -Wall -Wextra -Wno-deprecated-declarations -pedantic-errors)
    add_test(NAME ice-ingress-options COMMAND ice-ingress-options-tests)
    add_executable(ice-source-client-fixture "${REPOSITORY}/tests/ice_source_client_fixture.cpp")
    target_link_libraries(ice-source-client-fixture PRIVATE getdaq-ice-source)
    set_target_properties(ice-source-client-fixture PROPERTIES CXX_EXTENSIONS OFF RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    target_compile_options(ice-source-client-fixture PRIVATE -Wall -Wextra -Wno-deprecated-declarations -pedantic-errors)
    add_executable(ice-ingress-fixture "${REPOSITORY}/tests/ice_ingress_fixture.cpp")
    target_link_libraries(ice-ingress-fixture PRIVATE getdaq-ice-ingress-protocol)
    set_target_properties(ice-ingress-fixture PROPERTIES CXX_EXTENSIONS OFF RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    target_compile_options(ice-ingress-fixture PRIVATE -Wall -Wextra -Wno-deprecated-declarations -pedantic-errors)
endif()
add_subdirectory("${REPOSITORY}/apps/icecontrol" "controlled-source")
if(BUILD_TESTING AND TARGET ice-source-control-tests)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME ice-source-control-lifecycle
            COMMAND ${Python3_EXECUTABLE} "${REPOSITORY}/tests/run_ice_source_control_tests.py"
                    "$<TARGET_FILE:getdaq-ice-receiver>"
                    "$<TARGET_FILE:ice-source-control-tests>"
                    "${CMAKE_BINARY_DIR}/ice-source-control-evidence")
        set_tests_properties(ice-source-control-lifecycle PROPERTIES TIMEOUT 360)
    endif()
endif()
