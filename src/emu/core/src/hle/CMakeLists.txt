add_library(hle 
    ${CORE_INCLUDE_DIR}/hle/libmanager.h
    libmanager.cpp)

target_link_libraries(hle PRIVATE yaml-cpp common)
target_include_directories(hle PRIVATE ${YAML_INCLUDE_DIR})
target_include_directories(hle PUBLIC  ${CORE_INCLUDE_DIR})