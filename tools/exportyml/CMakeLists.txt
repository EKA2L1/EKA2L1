add_executable(exportyml src/main.cpp)

target_link_libraries(exportyml
    PRIVATE
    common
    yaml-cpp
    stdc++fs)

target_include_directories(exportyml
    PUBLIC
    ${YAML_INCLUDE_DIR})
