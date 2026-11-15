find_package(Qt5 5.12 REQUIRED COMPONENTS Core Network Concurrent)
add_executable(getdaq-supervisor main.cpp supervisorservice.cpp supervisorservice.h
  "${CMAKE_CURRENT_SOURCE_DIR}/../../../common/http/httpserver.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../../../common/http/httpserver.h")
target_include_directories(getdaq-supervisor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../common/http")
target_link_libraries(getdaq-supervisor PRIVATE getdaq-data-supervisor-core Qt5::Core Qt5::Network Qt5::Concurrent)
target_compile_options(getdaq-supervisor PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(getdaq-supervisor PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
