add_executable(e32uncompress
    src/main.cpp)

target_link_libraries(e32uncompress PRIVATE common epocloader)

set_target_properties(e32uncompress PROPERTIES OUTPUT_NAME e32uncompress
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools")
