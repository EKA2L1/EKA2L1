# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.8)

add_library(native-lib SHARED
        include/android/emu_window_android.h
        include/android/launcher.h
        include/android/state.h
        include/android/thread.h
        src/emu_window_android.cpp
        src/launcher.cpp
        src/state.cpp
        src/thread.cpp
        src/native-lib.cpp)

target_link_libraries(native-lib PRIVATE
        android
        common
        cpu
        drivers
        EGL
        glad
        epoc
        epockern
        epocpkg
        epocservs
        yaml-cpp)

target_include_directories(native-lib PRIVATE include)

add_dependencies(native-lib scdv mediaclientaudio mediaclientaudiostream)

set(ASSETS_DIR "${PROJECT_SOURCE_DIR}/src/emu/android/app/src/main/assets")

add_custom_command(
        TARGET native-lib
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS_DIR}/compat"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS_DIR}/patch"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/bin/patch/" "${ASSETS_DIR}/patch"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/config/compat/" "${ASSETS_DIR}/compat")
