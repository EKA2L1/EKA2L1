add_executable(gdrdump
    src/main.cpp)

target_link_libraries(gdrdump PRIVATE common epocloader)

set_target_properties(gdrdump PROPERTIES OUTPUT_NAME gdrdump
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools")
