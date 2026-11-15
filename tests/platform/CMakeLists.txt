cmake_minimum_required(VERSION 3.12)
project(GetDAQPlatformValidation LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
get_filename_component(GETDAQ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
# One shared experiment core, plus protocol/state/journal/receiver contracts.
# No Qt/Ice library is needed by these native contracts.
add_subdirectory("${GETDAQ_ROOT}/common/control" "control")
add_subdirectory("${GETDAQ_ROOT}/apps/ecc" "ecc")
add_subdirectory("${GETDAQ_ROOT}/apps/datareceiver" "receiver")
