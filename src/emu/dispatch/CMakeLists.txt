
add_library(epocdispatch
        include/dispatch/libraries/egl/def.h
        include/dispatch/libraries/egl/egl.h
        include/dispatch/libraries/gles_shared/consts.h
        include/dispatch/libraries/gles_shared/def.h
        include/dispatch/libraries/gles_shared/gles_shared.h
        include/dispatch/libraries/gles_shared/utils.h
        include/dispatch/libraries/gles1/def.h
        include/dispatch/libraries/gles1/gles1.h
        include/dispatch/libraries/gles1/shadergen.h
        include/dispatch/libraries/gles1/shaderman.h
        include/dispatch/libraries/gles2/def.h
        include/dispatch/libraries/gles2/gles2.h
        include/dispatch/libraries/gles2/register.h
        include/dispatch/libraries/sysutils/functions.h
        include/dispatch/libraries/sysutils/register.h
        include/dispatch/libraries/register.h
        include/dispatch/audio.h
        include/dispatch/camera.h
        include/dispatch/def.h
        include/dispatch/dispatcher.h
        include/dispatch/hui.h
        include/dispatch/management.h
        include/dispatch/register.h
        include/dispatch/screen.h
        include/dispatch/video.h
        src/libraries/egl/def.cpp
        src/libraries/egl/egl.cpp
        src/libraries/gles_shared/gles_shared.cpp
        src/libraries/gles1/gles1.cpp
        src/libraries/gles1/shadergen.cpp
        src/libraries/gles1/shaderman.cpp
        src/libraries/gles2/gles2.cpp
        src/libraries/sysutils/functions.cpp
        src/libraries/register.cpp
        src/audio.cpp
        src/camera.cpp
        src/dispatcher.cpp
        src/hui.cpp
        src/register.cpp
        src/screen.cpp
        src/video.cpp)

target_include_directories(epocdispatch PUBLIC include)

target_link_libraries(epocdispatch PUBLIC
        bridge
        common)

target_link_libraries(epocdispatch PRIVATE
        drivers
        epoc
        epockern
        epocpkg
        epocservs
        glm
        xxHash)