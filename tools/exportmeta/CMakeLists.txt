add_executable(exportmeta src/main.cpp)
target_link_libraries(exportmeta PRIVATE
     common libclang yaml-cpp)

target_include_directories(exportmeta
    PUBLIC
    ${YAML_INCLUDE_DIR})

if (NOT MSVC)
 target_link_libraries(exportmeta PRIVATE stdc++fs)
endif()

if (NOT WIN32)
 target_link_libraries(exportmeta PRIVATE pthread)
endif()
