add_executable(rofsdump
    src/main.cpp)

target_link_libraries(rofsdump PRIVATE common epocloader)

set_target_properties(rofsdump PROPERTIES OUTPUT_NAME rofsdump
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools")
