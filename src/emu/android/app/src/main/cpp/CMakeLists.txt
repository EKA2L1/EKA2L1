# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.8)

add_library(native-lib SHARED
        include/android/emu_window_android.h
        include/android/input_dialog.h
        include/android/launcher.h
        include/android/state.h
        include/android/thread.h
        src/emu_window_android.cpp
        src/input_dialog.cpp
        src/launcher.cpp
        src/state.cpp
        src/thread.cpp
        src/native-lib.cpp)

target_link_libraries(native-lib PRIVATE
        android
        Catch2
        common
        cpu
        drivers
        EGL
        glad
        jnigraphics
        lunasvg
        epoc
        epockern
        epocpkg
        epocservs
        yaml-cpp)

target_include_directories(native-lib PRIVATE include)
add_dependencies(native-lib
        audiooutputrouting
        avkonfep
        ecam
        scdv
        mediaclientaudio
        mediaclientaudiostream
        mediaclientvideo
        vibractrl
        backlightctrl
        postingsurfacefactory
        goommonitor)

set(ASSETS_DIR "${PROJECT_SOURCE_DIR}/src/emu/android/app/src/main/assets")

if (ENABLE_SCRIPTING)
    message("Enable scripting on Android build")

    target_link_libraries(native-lib PRIVATE -Wl,--whole-archive $<TARGET_FILE:scripting> -Wl,--no-whole-archive)

    add_custom_command(
            TARGET native-lib
            PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS_DIR}/scripts"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/bin/scripts/" "${ASSETS_DIR}/scripts")
endif()

add_custom_command(
        TARGET native-lib
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS_DIR}/scripts"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS_DIR}/scripts/disabled"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS_DIR}/compat"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS_DIR}/resources"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS_DIR}/resources/upscale"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS_DIR}/patch"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/bin/patch/" "${ASSETS_DIR}/patch"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/drivers/resources/gles/" "${ASSETS_DIR}/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/drivers/resources/upscale/" "${ASSETS_DIR}/resources/upscale"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/config/compat/" "${ASSETS_DIR}/compat"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/scripts/" "${ASSETS_DIR}/scripts/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/emu/drivers/resources/defaultbank.hsb" "${ASSETS_DIR}/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/emu/drivers/resources/defaultbank.sf2" "${ASSETS_DIR}/resources")

if (${CMAKE_ANDROID_ARCH_ABI} MATCHES "arm64-v8a")
    target_link_options(native-lib PRIVATE "-Wl,-Bsymbolic")
endif()