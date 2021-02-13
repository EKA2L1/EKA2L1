if (WIN32)
    set(CONSOLE_RESOURCE console.rc)
endif ()

add_executable(console
        include/console/cmdhandler.h
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
        epocpkg
        epocservs
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
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:console>/compat"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/panic/panic.json" "$<TARGET_FILE_DIR:console>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/panic/domaincli.py" "$<TARGET_FILE_DIR:console>/scripts/disabled/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/utils/leavehook.py" "$<TARGET_FILE_DIR:console>/scripts/disabled/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/console/resources" "$<TARGET_FILE_DIR:console>/resources/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/drivers/resources/" "$<TARGET_FILE_DIR:console>/resources/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/debugger/resources/" "$<TARGET_FILE_DIR:console>/resources/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/debugger/assets/" "$<TARGET_FILE_DIR:console>/resources/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/config/compat/" "$<TARGET_FILE_DIR:console>/compat/")

if (EKA2L1_ENABLE_SCRIPTING_ABILITY)
        add_custom_command(
                TARGET console
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LUA_DLL_PATH}" "$<TARGET_FILE_DIR:console>"
        )
        
        set_target_properties(console PROPERTIES ENABLE_EXPORTS 1)

        if (UNIX AND (NOT APPLE) AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"))
            target_link_libraries(console PRIVATE -Wl,--whole-archive $<TARGET_FILE:symemu> -Wl,--no-whole-archive)
        elseif(MSVC)
            set_target_properties(console PROPERTIES LINK_FLAGS /WHOLEARCHIVE:symemu.lib)
        else()
            target_link_libraries(console PRIVATE symemu)
        endif()
endif()
