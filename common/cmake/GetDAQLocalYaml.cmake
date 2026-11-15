# 原生组件共享同一份已嵌入的 yaml-cpp，独立构建也不联网获取依赖。
if(NOT TARGET getdaq-yaml-native)
  get_filename_component(getdaq_yaml_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
  file(GLOB getdaq_yaml_sources "${getdaq_yaml_root}/third_party/yaml-cpp/src/*.cpp"
                               "${getdaq_yaml_root}/third_party/yaml-cpp/src/contrib/*.cpp")
  add_library(getdaq-yaml-native STATIC ${getdaq_yaml_sources})
  target_include_directories(getdaq-yaml-native PUBLIC "${getdaq_yaml_root}/third_party/yaml-cpp/include"
                             PRIVATE "${getdaq_yaml_root}/third_party/yaml-cpp/src")
  target_compile_definitions(getdaq-yaml-native PUBLIC YAML_CPP_STATIC_DEFINE)
endif()
