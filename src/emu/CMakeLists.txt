project (EKA2L1)

add_subdirectory(common)
add_subdirectory(imgui)
add_subdirectory(core)

add_executable(EKA2L1_CONS console/main.cpp)
target_link_libraries(EKA2L1_CONS PRIVATE common core)

set_target_properties(EKA2L1_CONS PROPERTIES OUTPUT_NAME eka2l1_cons
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
	
add_custom_command(
	TARGET EKA2L1_CONS
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory "${ROOT}/sid" "$<TARGET_FILE_DIR:EKA2L1_CONS>/sid")