cmake_minimum_required(VERSION 3.12)
project(GetDAQControlMcp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt5 5.12 REQUIRED COMPONENTS Core Network)

add_executable(getdaq-control-mcp main.cpp mcpserver_protocol.cpp mcpserver_transport.cpp mcpserver_readtools.cpp mcpserver_controltools.cpp mcpserver_p.h mcpsettings.cpp mcpsettings.h mcptoolschema.cpp mcptoolschema.h mcpstdio.cpp mcpstdio.h)
target_link_libraries(getdaq-control-mcp PRIVATE Qt5::Core Qt5::Network)
target_compile_options(getdaq-control-mcp PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME ecc-control-mcp-stdio
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../../tests/ecc_mcp_test.py"
              "$<TARGET_FILE:getdaq-control-mcp>")
    set_tests_properties(ecc-control-mcp-stdio PROPERTIES TIMEOUT 30)
  endif()
endif()
