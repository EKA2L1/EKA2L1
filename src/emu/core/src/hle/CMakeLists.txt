add_library(hle 
    ${CORE_INCLUDE_DIR}/hle/libmanager.h
    libmanager.cpp)

target_link_libraries(hle PUBLIC yaml-cpp common)
target_link_libraries(hle PRIVATE coreloader)
target_include_directories(hle PUBLIC ${YAML_INCLUDE_DIR})
target_include_directories(hle PUBLIC  ${CORE_INCLUDE_DIR})