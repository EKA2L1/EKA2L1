if (WIN32)
    set(CONSOLE_RESOURCE console.rc)
endif ()

add_executable(console
        include/console/cmdhandler.h
        include/console/global.h
        include/console/thread.h
        include/console/seh_handler.h
        src/cmdhandler.cpp
        src/state.cpp
        src/thread.cpp
        src/main.cpp
        src/seh_handler.cpp
        ${CONSOLE_RESOURCE})

target_link_libraries(console PRIVATE
        common
        cpu
        debugger
        drivers
        epoc
        epockern
        epocservs
        manager
        gdbstub
        imgui
        yaml-cpp)

target_include_directories(console PRIVATE include ${YAML_CPP_INCLUDE_DIR})

set_target_properties(console PROPERTIES OUTPUT_NAME eka2l1
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin")

add_dependencies(console scdv mediaclientaudio mediaclientaudiostream)

add_custom_command(
        TARGET console
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:console>/scripts"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:console>/scripts/disabled"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:console>/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/panic/panic.json" "$<TARGET_FILE_DIR:console>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/panic/domaincli.py" "$<TARGET_FILE_DIR:console>/scripts/disabled/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/utils/leavehook.py" "$<TARGET_FILE_DIR:console>/scripts/disabled/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/console/resources" "$<TARGET_FILE_DIR:console>/resources/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/drivers/resources/" "$<TARGET_FILE_DIR:console>/resources/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/debugger/assets/" "$<TARGET_FILE_DIR:console>/resources/")
