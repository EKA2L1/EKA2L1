set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(ANDROID)
    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    if (ANDROID_ABI STREQUAL "armeabi-v7a")
        set(ANDROID_EXTRA_LIBS
            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
    endif()
endif()

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets LinguistTools REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets LinguistTools OpenGLWidgets Svg REQUIRED)

set(TS_FILES translations/eka2l1_en_US.ts)
set(RESOURCE_FILES eka2l1.qrc assets/themes/dark/style.qrc)

if (WIN32)
    set(RESOURCE_FILES ${RESOURCE_FILES} eka2l1.rc)
endif()

set(PROJECT_SOURCES
        include/qt/aboutdialog.h
        include/qt/applistwidget.h
        include/qt/cmdhandler.h
        include/qt/device_install_dialog.h
        include/qt/displaywidget.h
        include/qt/mainwindow.h
        include/qt/package_manager_dialog.h
        include/qt/seh_handler.h
        include/qt/settings_dialog.h
        include/qt/state.h
        include/qt/thread.h
        include/qt/utils.h
        src/aboutdialog.cpp
        src/aboutdialog.ui
        src/applistwidget.cpp
        src/cmdhandler.cpp
        src/device_install_dialog.cpp
        src/device_install_dialog.ui
        src/displaywidget.cpp
        src/main.cpp
        src/mainwindow.cpp
        src/mainwindow.ui
        src/package_manager_dialog.cpp
        src/package_manager_dialog.ui
        src/seh_handler.cpp
        src/settings_dialog.cpp
        src/settings_dialog.ui
        src/state.cpp
        src/thread.cpp
        src/utils.cpp
        ${RESOURCE_FILES}
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(eka2l1_qt
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )

    qt_create_translation(QM_FILES ${CMAKE_CURRENT_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(eka2l1_qt SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(eka2l1_qt
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_include_directories(eka2l1_qt PUBLIC include)

target_link_libraries(eka2l1_qt PRIVATE
    Qt${QT_VERSION_MAJOR}::OpenGLWidgets
    Qt${QT_VERSION_MAJOR}::Svg
    Qt${QT_VERSION_MAJOR}::Widgets
    common
    config
    drivers
    epoc
    epocservs
    epockern
    epocpkg
    gdbstub
    yaml-cpp)

set_target_properties(eka2l1_qt PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.github.eka2l1
    MACOSX_BUNDLE_BUNDLE_VERSION 0.0.4.2
    MACOSX_BUNDLE_SHORT_VERSION_STRING 0.0.4.2
)

set_target_properties(eka2l1_qt PROPERTIES OUTPUT_NAME eka2l1_qt
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin")

add_dependencies(eka2l1_qt scdv mediaclientaudio mediaclientaudiostream)

add_custom_command(
        TARGET eka2l1_qt
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:eka2l1_qt>/scripts"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:eka2l1_qt>/scripts/disabled"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:eka2l1_qt>/resources"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:eka2l1_qt>/compat"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/panic/panic.json" "$<TARGET_FILE_DIR:eka2l1_qt>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/panic/domaincli.py" "$<TARGET_FILE_DIR:eka2l1_qt>/scripts/disabled/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/utils/leavehook.py" "$<TARGET_FILE_DIR:eka2l1_qt>/scripts/disabled/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/drivers/resources/" "$<TARGET_FILE_DIR:eka2l1_qt>/resources/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/config/compat/" "$<TARGET_FILE_DIR:eka2l1_qt>/compat/")

if (EKA2L1_ENABLE_SCRIPTING_ABILITY)
        add_custom_command(
                TARGET eka2l1_qt
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LUA_DLL_PATH}" "$<TARGET_FILE_DIR:eka2l1_qt>"
        )

        set_target_properties(eka2l1_qt PROPERTIES ENABLE_EXPORTS 1)

        if (UNIX AND (NOT APPLE) AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"))
            target_link_libraries(eka2l1_qt PRIVATE -Wl,--whole-archive $<TARGET_FILE:symemu> -Wl,--no-whole-archive)
        elseif(MSVC)
            set_target_properties(eka2l1_qt PROPERTIES LINK_FLAGS /WHOLEARCHIVE:symemu.lib)
        else()
            target_link_libraries(eka2l1_qt PRIVATE symemu)
        endif()
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(eka2l1_qt)
endif()

if (WIN32)
    get_target_property(QMAKE_EXE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QMAKE_EXE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    add_custom_command(
        TARGET eka2l1_qt
        POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-compiler-runtime --no-opengl-sw --no-quick --plugindir "$<TARGET_FILE_DIR:eka2l1_qt>/qt/plugins" --release "$<TARGET_FILE:eka2l1_qt>")
endif()
