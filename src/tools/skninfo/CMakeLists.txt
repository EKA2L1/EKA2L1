add_executable(skninfo
    src/main.cpp)

target_link_libraries(skninfo PRIVATE common epocservs)

set_target_properties(skninfo PROPERTIES OUTPUT_NAME skninfo
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools")
