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
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets LinguistTools OpenGLWidgets REQUIRED)

set(TS_FILES translations/eka2l1_en_US.ts)
set(RESOURCE_FILES eka2l1.qrc)

set(PROJECT_SOURCES
        include/qt/aboutdialog.h
        include/qt/applistwidget.h
        include/qt/cmdhandler.h
        include/qt/mainwindow.h
        include/qt/seh_handler.h
        include/qt/state.h
        include/qt/thread.h
        src/aboutdialog.cpp
        src/aboutdialog.ui
        src/applistwidget.cpp
        src/cmdhandler.cpp
        src/main.cpp
        src/mainwindow.cpp
        src/mainwindow.ui
        src/seh_handler.cpp
        src/state.cpp
        src/thread.cpp
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
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGLWidgets
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

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(eka2l1_qt)
endif()
