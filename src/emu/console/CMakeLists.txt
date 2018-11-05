add_executable(eka2l1 src/main.cpp)
target_link_libraries(eka2l1 PRIVATE common core debugger drivers 
	gdbstub imgui yaml-cpp)

target_include_directories(eka2l1 PRIVATE ${YAML_CPP_INCLUDE_DIR})

set_target_properties(eka2l1 PROPERTIES OUTPUT_NAME eka2l1
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(etests src/test.cpp)

target_link_libraries(etests PRIVATE common core 
	gdbstub imgui yaml-cpp)

target_include_directories(etests PRIVATE ${YAML_CPP_INCLUDE_DIR})

set_target_properties(etests PROPERTIES OUTPUT_NAME etests
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
	
if (WIN32)
	add_custom_command(
			TARGET eka2l1
			POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/unicorn/windows/unicorn.dll" "$<TARGET_FILE_DIR:eka2l1>"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/unicorn/windows/libgcc_s_seh-1.dll" "$<TARGET_FILE_DIR:eka2l1>"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/unicorn/windows/libwinpthread-1.dll" "$<TARGET_FILE_DIR:eka2l1>")
endif()

add_custom_command(
	TARGET eka2l1
			POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:eka2l1>/scripts"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/panic/panic.json" "$<TARGET_FILE_DIR:eka2l1>"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/panic/domaincli.py" "$<TARGET_FILE_DIR:eka2l1>/scripts/"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/utils/leavehook.py" "$<TARGET_FILE_DIR:eka2l1>/scripts/")