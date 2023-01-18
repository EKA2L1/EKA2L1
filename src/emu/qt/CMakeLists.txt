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

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets LinguistTools Gui REQUIRED)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    set(EXCLUSIVE_QT_COMPONENTS OpenGLWidgets)
else()
    set(EXCLUSIVE_QT_COMPONENTS OpenGL)
endif()

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets LinguistTools Svg Network ${EXCLUSIVE_QT_COMPONENTS} REQUIRED)

# Make list of TS files, and add them to resources. Credits from Yuzu CMakeLists
set(TS_FOLDER translations)

file(GLOB_RECURSE TS_FILES ${TS_FOLDER}/*.ts)
list(REMOVE_ITEM TS_FILES ${TS_FOLDER}/eka2l1.ts)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_create_translation(QM_FILES ${CMAKE_CURRENT_SOURCE_DIR} ${TS_FILES})
else()
    qt5_create_translation(QM_FILES ${CMAKE_CURRENT_SOURCE_DIR} ${TS_FILES})
endif()

set(LANGUAGE_QRC_FILE ${CMAKE_CURRENT_BINARY_DIR}/languages.qrc)
file(WRITE ${LANGUAGE_QRC_FILE} "<RCC><qresource prefix=\"languages\">\n")
foreach (QM ${QM_FILES})
    get_filename_component(QM_FILE ${QM} NAME)
    file(APPEND ${LANGUAGE_QRC_FILE} "<file>${QM_FILE}</file>\n")
endforeach (QM)
file(APPEND ${LANGUAGE_QRC_FILE} "</qresource></RCC>")

set(RESOURCE_FILES eka2l1.qrc assets/themes/dark/style.qrc ${CMAKE_CURRENT_BINARY_DIR}/languages.qrc)

if (WIN32)
    set(RESOURCE_FILES ${RESOURCE_FILES} eka2l1.rc)
endif()

set(PROJECT_SOURCES
        include/qt/btnmap/aim.h
        include/qt/btnmap/base.h
        include/qt/btnmap/editor.h
        include/qt/btnmap/editor_widget.h
        include/qt/btnmap/executor.h
        include/qt/btnmap/joystick.h
        include/qt/btnmap/singletouch.h
        include/qt/aboutdialog.h
        include/qt/applistwidget.h
        include/qt/btnetplay_friends_dialog.h
        include/qt/btnet_dialog.h
        include/qt/cmdhandler.h
        include/qt/device_install_dialog.h
        include/qt/displaywidget.h
        include/qt/input_dialog_driver.h
        include/qt/launch_process_dialog.h
        include/qt/mainwindow.h
        include/qt/package_manager_dialog.h
        include/qt/seh_handler.h
        include/qt/settings_dialog.h
        include/qt/state.h
        include/qt/symbian_input_dialog.h
        include/qt/thread.h
        include/qt/update_dialog.h
        include/qt/update_notice_dialog.h
        include/qt/utils.h
        src/btnmap/aim.cpp
        src/btnmap/base.cpp
        src/btnmap/editor.cpp
        src/btnmap/editor_widget.cpp
        src/btnmap/editor_widget.ui
        src/btnmap/executor.cpp
        src/btnmap/joystick.cpp
        src/btnmap/singletouch.cpp
        src/aboutdialog.cpp
        src/aboutdialog.ui
        src/applistwidget.cpp
        src/btnetplay_friends_dialog.cpp
        src/btnet_dialog.cpp
        src/btnet_dialog.ui
        src/cmdhandler.cpp
        src/common_library_plugin.cpp
        src/device_install_dialog.cpp
        src/device_install_dialog.ui
        src/displaywidget.cpp
        src/input_dialog_driver.cpp
        src/launch_process_dialog.cpp
        src/launch_process_dialog.ui
        src/main.cpp
        src/mainwindow.cpp
        src/mainwindow.ui
        src/package_manager_dialog.cpp
        src/package_manager_dialog.ui
        src/seh_handler.cpp
        src/settings_dialog.cpp
        src/settings_dialog.ui
        src/state.cpp
        src/symbian_input_dialog.cpp
        src/symbian_input_dialog.ui
        src/thread.cpp
        src/update_dialog.cpp
        src/update_dialog.ui
        src/update_notice_dialog.cpp
        src/update_notice_dialog.ui
        src/utils.cpp
        ${RESOURCE_FILES}
)

set(UPDATE_SOURCES
        src/updater.cpp
        src/updater.ui
        ${RESOURCE_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(eka2l1_qt
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )

    if (WIN32)
        qt_add_executable(updater
            WIN32
            MANUAL_FINALIZATION
            ${UPDATE_SOURCES}
        )
    endif()
else()
    if(ANDROID)
        add_library(eka2l1_qt SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(eka2l1_qt
            ${PROJECT_SOURCES}
        )
        if (WIN32)
            add_executable(updater
                WIN32
                ${UPDATE_SOURCES}
            )
        endif()
    endif()
endif()

target_include_directories(eka2l1_qt PUBLIC include)

if (${QT_VERSION_MAJOR} EQUAL 6)
    target_include_directories(eka2l1_qt PRIVATE ${Qt6Gui_PRIVATE_INCLUDE_DIRS})
    if (WIN32)
        target_include_directories(updater PRIVATE ${Qt6Gui_PRIVATE_INCLUDE_DIRS})
    endif()
else()
    target_include_directories(eka2l1_qt PRIVATE ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
    if (WIN32)
        target_include_directories(updater PRIVATE ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
    endif()
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    set(EXCLUSIVE_QT_LIBRARIES
        Qt${QT_VERSION_MAJOR}::OpenGLWidgets)
else()
    set(EXCLUSIVE_QT_LIBRARIES
        Qt${QT_VERSION_MAJOR}::OpenGL)
endif()

target_link_libraries(eka2l1_qt PRIVATE
    Qt${QT_VERSION_MAJOR}::Svg
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    ${EXCLUSIVE_QT_LIBRARIES}
    common
    config
    drivers
    epoc
    epocservs
    epockern
    epocpkg
    gdbstub
    j2me
    yaml-cpp)
	
set_target_properties(eka2l1_qt PROPERTIES OUTPUT_NAME eka2l1_qt
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin")

if (WIN32)
    target_link_libraries(updater PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        common)

    set_target_properties(updater PROPERTIES OUTPUT_NAME updater
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/updater"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/updater"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/updater"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/updater"
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/updater")
endif()

add_dependencies(eka2l1_qt
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

add_custom_command(
        TARGET eka2l1_qt
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:eka2l1_qt>/scripts"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:eka2l1_qt>/scripts/disabled"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:eka2l1_qt>/resources"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:eka2l1_qt>/resources/upscale"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:eka2l1_qt>/compat"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/panic/panic.json" "$<TARGET_FILE_DIR:eka2l1_qt>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/miscs/utils/leavehook.py" "$<TARGET_FILE_DIR:eka2l1_qt>/scripts/disabled/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/scripts/" "$<TARGET_FILE_DIR:eka2l1_qt>/scripts/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/drivers/resources/gl/" "$<TARGET_FILE_DIR:eka2l1_qt>/resources/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/drivers/resources/upscale/" "$<TARGET_FILE_DIR:eka2l1_qt>/resources/upscale/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/emu/drivers/resources/defaultbank.hsb" "$<TARGET_FILE_DIR:eka2l1_qt>/resources/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/emu/drivers/resources/defaultbank.sf2" "$<TARGET_FILE_DIR:eka2l1_qt>/resources/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/miscs/compat/" "$<TARGET_FILE_DIR:eka2l1_qt>/compat/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/SDL_GameControllerDB/gamecontrollerdb.txt" "$<TARGET_FILE_DIR:eka2l1_qt>/resources/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/emu/qt/resources/" "$<TARGET_FILE_DIR:eka2l1_qt>/resources/"
        )

if (EKA2L1_ENABLE_SCRIPTING_ABILITY)
        add_custom_command(
                TARGET eka2l1_qt
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LUA_DLL_PATH}" "$<TARGET_FILE_DIR:eka2l1_qt>"
        )

        set_target_properties(eka2l1_qt PROPERTIES ENABLE_EXPORTS 1)

        if (UNIX AND (NOT APPLE) AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"))
            target_link_libraries(eka2l1_qt PRIVATE -Wl,--whole-archive $<TARGET_FILE:scripting> -Wl,--no-whole-archive)
        elseif(MSVC)
            set_target_properties(eka2l1_qt PROPERTIES LINK_FLAGS /WHOLEARCHIVE:scripting.lib)
        else()
            target_link_libraries(eka2l1_qt PRIVATE scripting)
        endif()
endif()

get_target_property(QMAKE_EXE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
get_filename_component(QT_BIN_DIR "${QMAKE_EXE}" DIRECTORY)

if(APPLE)
    set_target_properties(eka2l1_qt PROPERTIES
       MACOSX_BUNDLE True
       MACOSX_BUNDLE_GUI_IDENTIFIER my.github.eka2l1
       MACOSX_BUNDLE_BUNDLE_VERSION 0.0.4.2
       MACOSX_BUNDLE_SHORT_VERSION_STRING 0.0.4.2
       MACOSX_BUNDLE_ICON_FILE duck.icns
       XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/EKA2L1.entitlements"
       XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--deep --options=runtime"
       OUTPUT_NAME EKA2L1
    )

    # Copy Qt plugins into the bundle
    if (${QT_VERSION_MAJOR} EQUAL 5)
        get_target_property(qtcocoa_location Qt${QT_VERSION_MAJOR}::QCocoaIntegrationPlugin LOCATION)
        target_sources(eka2l1_qt PRIVATE "${qtcocoa_location}")
        set_source_files_properties("${qtcocoa_location}" PROPERTIES MACOSX_PACKAGE_LOCATION MacOS/platforms)

        get_target_property(qtmacstyle_location Qt${QT_VERSION_MAJOR}::QMacStylePlugin LOCATION)
        target_sources(eka2l1_qt PRIVATE "${qtmacstyle_location}")
        set_source_files_properties("${qtmacstyle_location}" PROPERTIES MACOSX_PACKAGE_LOCATION MacOS/styles)
    endif()

    # Copy icon into the bundle
    target_sources(eka2l1_qt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/duck.icns")
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/duck.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    add_custom_command(
        TARGET eka2l1_qt
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:eka2l1_qt>/patch"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/bin/patch/" "$<TARGET_FILE_DIR:eka2l1_qt>/patch/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/src/external/sdl2/macos/SDL2.framework" "$<TARGET_FILE_DIR:eka2l1_qt>/Frameworks/SDL2.framework")

    # Update library references to make the bundle portable
    include(DolphinPostprocessBundle)
    dolphin_postprocess_bundle(eka2l1_qt)
    # Fix rpath
    add_custom_command(TARGET eka2l1_qt
       POST_BUILD COMMAND
       ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "@executable_path/../Frameworks/"
       $<TARGET_FILE:eka2l1_qt>)

    if (EKA2L1_DEPLOY_DMG)
        find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${QT_BIN_DIR}")

        add_custom_command(TARGET eka2l1_qt POST_BUILD
            COMMAND "${MACDEPLOYQT_EXECUTABLE}"
                "$<TARGET_FILE_DIR:eka2l1_qt>/../.."
                -dmg
                -always-overwrite)

        add_custom_command(TARGET eka2l1_qt POST_BUILD
            COMMAND rm -rf "${CMAKE_BINARY_DIR}/bin/EKA2L1.app")
    endif()
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(eka2l1_qt)
    if (WIN32)
        qt_finalize_executable(updater)
    endif()
endif()

if (WIN32)
    set_property(TARGET eka2l1_qt PROPERTY WIN32_EXECUTABLE true)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    add_custom_command(
        TARGET eka2l1_qt
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/qt.conf.win" "$<TARGET_FILE_DIR:eka2l1_qt>/qt.conf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/src/external/sdl2/Windows/lib/x64/SDL2.dll" "$<TARGET_FILE_DIR:eka2l1_qt>")
    
    if(CMAKE_BUILD_TYPE MATCHES DEBUG)
        set(DEPLOY_BUILD_CONFIG_ARG "--debug")
    else()
        set(DEPLOY_BUILD_CONFIG_ARG "--release")
    endif()

    add_custom_command(
        TARGET eka2l1_qt
        POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-compiler-runtime --no-opengl-sw --no-quick --plugindir "$<TARGET_FILE_DIR:eka2l1_qt>/qt/plugins" ${DEPLOY_BUILD_CONFIG_ARG} "$<TARGET_FILE:eka2l1_qt>")

    set_property(TARGET updater PROPERTY WIN32_EXECUTABLE true)

    add_custom_command(
        TARGET updater
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/qt.conf.win" "$<TARGET_FILE_DIR:updater>/qt.conf")

    add_custom_command(
        TARGET updater
        POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-compiler-runtime --no-opengl-sw --no-quick --plugindir "$<TARGET_FILE_DIR:updater>/qt/plugins" ${DEPLOY_BUILD_CONFIG_ARG} "$<TARGET_FILE:updater>")
endif()

# This code is taken and modified from RPCS3
if (UNIX AND NOT APPLE)
    add_custom_command(
        TARGET eka2l1_qt
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/eka2l1.desktop" "$<TARGET_FILE_DIR:eka2l1_qt>/eka2l1.desktop")

    add_custom_command(
        TARGET eka2l1_qt
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/assets/duck_tank.png" "$<TARGET_FILE_DIR:eka2l1_qt>/duck_tank.png")

    add_custom_command(
        TARGET eka2l1_qt
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/assets/duck_tank_fedora.png" "$<TARGET_FILE_DIR:eka2l1_qt>/duck_tank_fedora.png")
endif()
