if (BUILD_WITH_VULKAN)
    set(DRIVERS_VULKAN_SRC
            include/drivers/graphics/backend/vulkan/graphics_vulkan.h
            src/graphics/backend/vulkan/graphics_vulkan.cpp)
endif ()

# Audio backends. iOS uses a native AURemoteIO / AVAudioSession driver instead
# of cubeb — cubeb's macOS AudioUnit code path leaks
# CoreAudio HAL types that the iOS SDK doesn't ship, so a direct
# implementation is both smaller and easier to keep in sync with the iOS
# audio APIs. Desktop / Android continue to use cubeb. FFmpeg is built
# separately for iOS by the ffmpeg submodule's ios-build.sh, then linked here when
# EKA2L1_HAVE_FFMPEG is enabled by src/external.
if (EKA2L1_IOS)
    set(DRIVERS_AUDIOUNIT_IOS_SRC
            include/drivers/audio/backend/audiounit_ios/audio_audiounit_ios.h
            include/drivers/audio/backend/audiounit_ios/stream_audiounit_ios.h
            src/audio/backend/audiounit_ios/audio_audiounit_ios.mm
            src/audio/backend/audiounit_ios/stream_audiounit_ios.mm)
    set(DRIVERS_HWRM_IOS_SRC
            include/drivers/hwrm/backend/vibration_ios.h
            src/hwrm/backend/vibration_ios.mm)
else()
    set(DRIVERS_CUBEB_SRC
            include/drivers/audio/backend/cubeb/audio_cubeb.h
            include/drivers/audio/backend/cubeb/stream_cubeb.h
            src/audio/backend/cubeb/audio_cubeb.cpp
            src/audio/backend/cubeb/stream_cubeb.cpp)
endif()

if (EKA2L1_HAVE_FFMPEG)
    set(DRIVERS_FFMPEG_SRC
            include/drivers/audio/backend/ffmpeg/dsp_ffmpeg.h
            include/drivers/audio/backend/ffmpeg/player_ffmpeg.h
            include/drivers/video/backend/ffmpeg/video_player_ffmpeg.h
            src/audio/backend/ffmpeg/dsp_ffmpeg.cpp
            src/audio/backend/ffmpeg/player_ffmpeg.cpp
            src/video/backend/ffmpeg/video_player_ffmpeg.cpp)
endif ()

# The OGL graphics backend was originally written against desktop GL via glad.
# On iOS we substitute ios_gl_loader.h for <glad/glad.h> in each ogl source
# and link OpenGLES.framework directly below — glad itself stays
# unbuilt.
set(DRIVERS_OGL_SRC
        include/drivers/graphics/backend/ogl/buffer_ogl.h
        include/drivers/graphics/backend/ogl/common_ogl.h
        include/drivers/graphics/backend/ogl/fb_ogl.h
        include/drivers/graphics/backend/ogl/graphics_ogl.h
        include/drivers/graphics/backend/ogl/input_desc_ogl.h
        include/drivers/graphics/backend/ogl/shader_ogl.h
        include/drivers/graphics/backend/ogl/texture_ogl.h
        src/graphics/backend/ogl/buffer_ogl.cpp
        src/graphics/backend/ogl/common_ogl.cpp
        src/graphics/backend/ogl/etcdec.cxx
        src/graphics/backend/ogl/fb_ogl.cpp
        src/graphics/backend/ogl/graphics_ogl.cpp
        src/graphics/backend/ogl/input_desc_ogl.cpp
        src/graphics/backend/ogl/pvrt-dec.cpp
        src/graphics/backend/ogl/texture_ogl.cpp
        src/graphics/backend/ogl/shader_ogl.cpp)
if (EKA2L1_IOS)
    list(APPEND DRIVERS_OGL_SRC
        include/drivers/graphics/backend/ogl/ios_gl_loader.h)
endif ()

add_library(drivers
        include/drivers/itc.h
        include/drivers/driver.h
        include/drivers/audio/audio.h
        include/drivers/audio/dsp.h
        include/drivers/audio/player.h
        include/drivers/audio/stream.h
        include/drivers/audio/backend/minibae/player_minibae.h
        include/drivers/audio/backend/tinysoundfont/player_tsf.h
        include/drivers/audio/backend/dsp_shared.h
        include/drivers/audio/backend/player_shared.h
        include/drivers/camera/camera_collection.h
        include/drivers/camera/camera_pixel.h
        include/drivers/hwrm/backend/vibration_null.h
        include/drivers/hwrm/vibration.h
        include/drivers/graphics/buffer.h
        include/drivers/graphics/emu_window.h
        include/drivers/graphics/fb.h
        include/drivers/graphics/context.h
        include/drivers/graphics/graphics.h
        include/drivers/graphics/input_desc.h
        include/drivers/graphics/shader.h
        include/drivers/graphics/texture.h
        include/drivers/graphics/backend/graphics_driver_shared.h
        include/drivers/input/emu_controller.h
        include/drivers/sensor/sensor.h
        include/drivers/video/video.h
        src/driver.cpp
        src/itc.cpp
        src/audio/audio.cpp
        src/audio/dsp.cpp
        src/audio/player.cpp
        src/audio/stream.cpp
        src/audio/backend/minibae/player_minibae.cpp
        src/audio/backend/tinysoundfont/player_tsf.cpp
        src/audio/backend/bae_platimpl.cpp
        src/audio/backend/dsp_shared.cpp
        src/audio/backend/player_shared.cpp
        src/camera/camera_collection.cpp
        src/camera/camera_pixel.cpp
        src/hwrm/backend/vibration_null.cpp
        src/hwrm/vibration.cpp
        src/input/common.cpp
        src/graphics/buffer.cpp
        src/graphics/context.cpp
        src/graphics/fb.cpp
        src/graphics/graphics.cpp
        src/graphics/input_desc.cpp
        src/graphics/shader.cpp
        src/graphics/texture.cpp
        src/graphics/backend/graphics_driver_shared.cpp
        src/sensor/backend/null/sensor_null.cpp
        src/sensor/sensor.cpp
        src/video/video.cpp
        ${DRIVERS_AUDIOUNIT_IOS_SRC}
        ${DRIVERS_CUBEB_SRC}
        ${DRIVERS_FFMPEG_SRC}
        ${DRIVERS_HWRM_IOS_SRC}
        ${DRIVERS_OGL_SRC}
        ${DRIVERS_VULKAN_SRC})
if (ANDROID)
    target_sources(drivers PRIVATE
        include/drivers/camera/backend/android/camera_android.h
        include/drivers/camera/backend/android/camera_collection_android.h
        include/drivers/hwrm/backend/vibration_jdk.h
        src/camera/backend/android/camera_android.cpp
        src/camera/backend/android/camera_collection_android.cpp
        src/camera/backend/android/emulator_camera_jni.cpp
        src/camera/backend/android/emulator_camera_jni.h
        src/hwrm/backend/vibration_jdk.cpp
        src/sensor/backend/android/sensor_android.cpp
        src/sensor/backend/android/sensor_android.h)
elseif (EKA2L1_IOS)
    # iOS sensor backend rides on CoreMotion (accelerometer, parity with the
    # Android backend), camera on AVFoundation. Input/window handling is
    # provided by the iOS frontend (no SDL2, no shared emu_window.cpp).
    target_sources(drivers PRIVATE
        include/drivers/camera/backend/ios/camera_ios.h
        include/drivers/camera/backend/ios/camera_pixel_ios.h
        include/drivers/camera/backend/ios/camera_simulator.h
        src/camera/backend/ios/camera_ios.mm
        src/camera/backend/ios/camera_pixel_ios.mm
        src/camera/backend/ios/camera_simulator.mm
        src/sensor/backend/ios/sensor_ios.h
        src/sensor/backend/ios/sensor_ios.mm)
else()
    target_sources(drivers PRIVATE
        include/drivers/input/backend/emu_controller_sdl2.h
        include/drivers/hwrm/backend/vibration_sdl2.h
        src/graphics/emu_window.cpp
        src/input/backend/emu_controller_sdl2.cpp
        src/input/emu_controller.cpp
        src/hwrm/backend/vibration_sdl2.cpp
        src/sdl2_scoping.cpp
        src/sdl2_scoping.h)

    # The desktop camera rides on Qt Multimedia, looked up in src/emu.
    if (Qt6Multimedia_FOUND)
        target_sources(drivers PRIVATE
            include/drivers/camera/backend/qt/camera_qt.h
            src/camera/backend/qt/camera_qt.cpp)

        target_link_libraries(drivers PRIVATE Qt6::Multimedia)
        target_compile_definitions(drivers PRIVATE EKA2L1_HAVE_QT_CAMERA=1)
        set_target_properties(drivers PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
endif()

if(WIN32)
    target_sources(drivers PRIVATE
        include/drivers/audio/backend/wmf/player_wmf.h
        include/drivers/audio/backend/wmf/wmf_loader.h
        src/audio/backend/wmf/wmf_loader.cpp
        src/audio/backend/wmf/player_wmf.cpp
        src/graphics/backend/context_wgl.cpp
        src/graphics/backend/context_wgl.h
    )

    target_link_libraries(drivers PRIVATE opengl32.lib Mfuuid.lib Propsys.lib)
elseif (EKA2L1_IOS)
    # EAGL-backed GLES3 context plus emu_window_ios.
    # The frontend's EAGLView hands a CAEAGLLayer in via emu_window_ios, which
    # surfaces it on window_system_info::render_surface for the context.
    target_sources(drivers PRIVATE
        src/graphics/backend/context_eagl.h
        src/graphics/backend/context_eagl.mm
        include/drivers/graphics/backend/emu_window_ios.h
        src/graphics/backend/emu_window_ios.mm
        src/ui/input_dialog_ios.mm
    )
elseif(APPLE)
    target_sources(drivers PRIVATE
        src/graphics/backend/context_agl.h
        src/graphics/backend/context_agl.mm
    )
elseif(HAIKU)
    target_sources(drivers PRIVATE
        src/graphics/backend/context_bgl.cpp
        src/graphics/backend/context_bgl.h
    )
elseif(ANDROID)
    target_sources(drivers PRIVATE
        src/graphics/backend/context_egl.cpp
        src/graphics/backend/context_egl.h
        src/graphics/backend/context_egl_android.cpp
        src/graphics/backend/context_egl_android.h
    )
else()
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(Wayland REQUIRED EGL)

    target_sources(drivers PRIVATE
        src/graphics/backend/context_glx.cpp
        src/graphics/backend/context_glx.h
        src/graphics/backend/context_egl.cpp
        src/graphics/backend/context_egl.h
        src/graphics/backend/context_wayland.cpp
        src/graphics/backend/context_wayland.h
    )

    # GLX has a hard dependency on libGL.
    # Make sure to link to it if using GLX.
    target_link_libraries(drivers PUBLIC ${OPENGL_LIBRARIES} ${X11_LIBRARIES} ${WAYLAND_LIBRARIES} EGL)
endif()

# Make miniBAE build alone so that its include directories don't messed up system headers
add_library(miniBAE_EMU STATIC ${MINIBAE_SHARED_SOURCES})
target_include_directories(miniBAE_EMU
    PRIVATE include/drivers/audio/backend/minibae ${MINIBAE_INTERNAL_INCLUDE_DIRS})
target_link_libraries(miniBAE_EMU PRIVATE common)

# miniBAE is old C that assigns integers to pointers and calls functions before
# declaring them. Clang 16 turned both into errors by default.
if (NOT MSVC)
    target_compile_options(miniBAE_EMU PRIVATE -Wno-int-conversion -Wno-implicit-function-declaration)
endif()

target_link_libraries(drivers PRIVATE common glm miniBAE_EMU)
if (EKA2L1_IOS)
    # glad is not linked on iOS — the ogl backend compiles against
    # OpenGLES.framework via ios_gl_loader.h. Audio goes
    # through the native AURemoteIO driver (DRIVERS_AUDIOUNIT_IOS_SRC).
    target_link_libraries(drivers PRIVATE
        "-framework OpenGLES"
        "-framework QuartzCore"
        "-framework Foundation"
        "-framework UIKit"
        "-framework AVFoundation"
        "-framework AudioToolbox"
        "-framework CoreAudio"
        "-framework CoreHaptics"
        "-framework CoreMotion"
        "-framework CoreMedia"
        "-framework CoreVideo"
        "-framework CoreGraphics"
        "-framework ImageIO")
else()
    target_link_libraries(drivers PRIVATE cubeb glad)
endif()
if (EKA2L1_HAVE_FFMPEG)
    target_link_libraries(drivers PRIVATE ffmpeg)
    target_compile_definitions(drivers PRIVATE EKA2L1_HAS_FFMPEG=1)
endif()
if (ANDROID)
    target_link_libraries(drivers PRIVATE android)
elseif (NOT EKA2L1_IOS)
    target_link_libraries(drivers PRIVATE SDL2)
endif()

target_include_directories(drivers PUBLIC include ${MINIBAE_PUBLIC_INCLUDE_DIRS} ${TSF_INCLUDE})

if (BUILD_WITH_VULKAN)
    target_link_libraries(drivers PRIVATE Vulkan)
endif ()
