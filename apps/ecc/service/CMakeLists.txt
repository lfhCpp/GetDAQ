find_package(Qt5 5.12 COMPONENTS Core Network Concurrent QUIET)
if(Qt5Core_FOUND AND Qt5Network_FOUND AND Qt5Concurrent_FOUND)
  add_executable(getdaq-ecc-server main.cpp eccservice.cpp eccservice_get.cpp eccservice_post.cpp eccservice.h eccservice_p.h
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../common/http/httpserver.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../../../common/http/httpserver.h")
  target_include_directories(getdaq-ecc-server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../common/http")
  target_link_libraries(getdaq-ecc-server PRIVATE getdaq-ecc-native Qt5::Core Qt5::Network Qt5::Concurrent)
  target_compile_options(getdaq-ecc-server PRIVATE -Wall -Wextra -Wpedantic -Wno-deprecated-declarations)
  set_target_properties(getdaq-ecc-server PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  add_executable(getdaq-handoff-service "${CMAKE_CURRENT_SOURCE_DIR}/../recovery/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../recovery/recoveryservice.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../recovery/recoveryservice.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../recovery/recoveryfiles.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../recovery/recoveryfiles_p.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../recovery/recoverytaskstore.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../recovery/recoverytaskstore_p.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../recovery/recoveryservicepolicy.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../recovery/recoveryservicepolicy_p.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../common/http/httpserver.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../../../common/http/httpserver.h")
  target_include_directories(getdaq-handoff-service PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../common/http")
  target_link_libraries(getdaq-handoff-service PRIVATE getdaq-ecc-native Qt5::Core Qt5::Network)
  target_compile_options(getdaq-handoff-service PRIVATE -Wall -Wextra -pedantic-errors -Wno-deprecated-declarations)
  set_target_properties(getdaq-handoff-service PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
else()
  message(STATUS "Qt Core/Network/Concurrent absent: native ECC library only; network service target omitted")
endif()
