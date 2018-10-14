if (WIN32)
    add_definitions(-DNOMINMAX)
    message("NOMINMAX")
endif()

add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(scripting)

add_executable(EKA2L1_CONS console/src/main.cpp)
target_link_libraries(EKA2L1_CONS PRIVATE common core gdbstub yaml-cpp)

target_include_directories(EKA2L1_CONS PRIVATE ${YAML_CPP_INCLUDE_DIR})

set_target_properties(EKA2L1_CONS PROPERTIES OUTPUT_NAME eka2l1_cons
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
	
if (WIN32)
	add_custom_command(
			TARGET EKA2L1_CONS
			POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/unicorn/windows/unicorn.dll" "$<TARGET_FILE_DIR:EKA2L1_CONS>"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/unicorn/windows/libgcc_s_seh-1.dll" "$<TARGET_FILE_DIR:EKA2L1_CONS>"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/unicorn/windows/libwinpthread-1.dll" "$<TARGET_FILE_DIR:EKA2L1_CONS>")
endif()

add_custom_command(
	TARGET EKA2L1_CONS
			POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:EKA2L1_CONS>/scripts"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/panic/panic.json" "$<TARGET_FILE_DIR:EKA2L1_CONS>"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/panic/domaincli.py" "$<TARGET_FILE_DIR:EKA2L1_CONS>/scripts/"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/utils/leavehook.py" "$<TARGET_FILE_DIR:EKA2L1_CONS>/scripts/")