add_executable(apiexample src/example.cpp)
target_link_libraries(apiexample PUBLIC eka2l1_api)

set_target_properties(apiexample PROPERTIES OUTPUT_NAME apiexample
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin") 