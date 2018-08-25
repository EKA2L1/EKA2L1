add_library(symemu
    include/scripting/cpu.h
    include/scripting/emulog.h
    include/scripting/instance.h
    include/scripting/process.h
    include/scripting/thread.h
    include/scripting/hook.h
    include/scripting/symemu.inl
    src/cpu.cpp
    src/emulog.cpp
    src/process.cpp
    src/hook.cpp
    src/thread.cpp
    src/instance.cpp)

pybind11_add_module(symemu_common
    include/scripting/common.h
    src/common.cpp)

target_include_directories(symemu PUBLIC include)
target_link_libraries(symemu PRIVATE common pybind11 core corekernel)

target_include_directories(symemu_common PUBLIC include)
target_link_libraries(symemu_common PRIVATE common pybind11)

# Move the syemu to script folder
set_target_properties(symemu PROPERTIES OUTPUT_NAME symemu
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/scripts"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/scripts"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/scripts")

set_target_properties(symemu_common PROPERTIES OUTPUT_NAME common
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/scripts/symemu2"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/scripts/symemu2"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/scripts/symemu2")

add_custom_command(
	TARGET symemu
			POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:symemu>/example"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/emu/scripting/README.md" "$<TARGET_FILE_DIR:symemu>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/emu/scripting/python/example/hello_eka2l1.py" "$<TARGET_FILE_DIR:symemu>/example/"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/emu/scripting/python/example/hello_panic.py" "$<TARGET_FILE_DIR:symemu>/example/"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/emu/scripting/python/example/hello_svc.py" "$<TARGET_FILE_DIR:symemu>/example/"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/emu/scripting/python/example/hello_reschedule.py" "$<TARGET_FILE_DIR:symemu>/example/")

add_custom_command(
	TARGET symemu_common
			POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/emu/scripting/python/symemu2/__init__.py" "$<TARGET_FILE_DIR:symemu_common>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/emu/scripting/python/symemu2/events.py" "$<TARGET_FILE_DIR:symemu_common>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/emu/scripting/python/symemu2/svc.py" "$<TARGET_FILE_DIR:symemu_common>")