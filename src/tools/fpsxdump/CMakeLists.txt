add_executable(fpsxdump
    src/main.cpp)

target_link_libraries(fpsxdump PRIVATE common epocloader)

set_target_properties(fpsxdump PROPERTIES OUTPUT_NAME fpsxdump
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools")
