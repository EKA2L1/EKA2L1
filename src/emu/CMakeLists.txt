add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(scripting)

add_executable(EKA2L1_CONS console/src/main.cpp)
target_link_libraries(EKA2L1_CONS PRIVATE common core yaml-cpp)

target_include_directories(EKA2L1_CONS PRIVATE ${YAML_CPP_INCLUDE_DIR})

set_target_properties(EKA2L1_CONS PROPERTIES OUTPUT_NAME eka2l1_cons
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
	
add_definitions(-DEKA2L1_API_EXPORT)

add_library(eka2l1_api SHARED core/include/core/core_api.h
					   core/src/core_api.cpp)

target_include_directories(eka2l1_api PUBLIC core/include)
target_link_libraries(eka2l1_api PUBLIC core common)

set_target_properties(eka2l1_api PROPERTIES OUTPUT_NAME eka2l1_api
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
