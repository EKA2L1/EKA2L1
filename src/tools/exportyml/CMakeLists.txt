add_executable(exportyml src/main.cpp)

target_link_libraries(exportyml
    PRIVATE
    common
    yaml-cpp)

target_include_directories(exportyml
    PUBLIC
    ${YAML_INCLUDE_DIR})

if (MSVC)
    target_link_libraries(exportyml
        PRIVATE
		libdemangle)
else()
    target_link_libraries(exportyml PRIVATE stdc++fs)
endif()

set_target_properties(exportyml PROPERTIES OUTPUT_NAME exportyml
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools")