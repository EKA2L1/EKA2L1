add_executable(exportlister src/main.cpp)
target_link_libraries(exportlister PRIVATE epocloader epocio common)

if (NOT MSVC)
    target_link_libraries(exportlister PRIVATE stdc++fs)
endif()

set_target_properties(exportlister PROPERTIES OUTPUT_NAME exportlister
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools")