add_executable(mbm2bmp src/main.cpp)
target_link_libraries(mbm2bmp PRIVATE common epocloader)

set_target_properties(mbm2bmp PROPERTIES OUTPUT_NAME mbm2bmp
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools")
