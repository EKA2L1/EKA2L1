/*
 * Copyright (c) 2019 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <common/log.h>
#include <common/raw_bind.h>
#include <android/emu_window_android.h>
#include <glad/glad.h>

static constexpr std::array<EGLint, 15> egl_attribs{EGL_SURFACE_TYPE,
                                                    EGL_WINDOW_BIT,
                                                    EGL_RENDERABLE_TYPE,
                                                    EGL_OPENGL_ES3_BIT_KHR,
                                                    EGL_BLUE_SIZE,
                                                    8,
                                                    EGL_GREEN_SIZE,
                                                    8,
                                                    EGL_RED_SIZE,
                                                    8,
                                                    EGL_DEPTH_SIZE,
                                                    0,
                                                    EGL_STENCIL_SIZE,
                                                    0,
                                                    EGL_NONE};
static constexpr std::array<EGLint, 15> egl_fake_attribs{EGL_SURFACE_TYPE,
                                                    EGL_PBUFFER_BIT,
                                                    EGL_RENDERABLE_TYPE,
                                                    EGL_OPENGL_ES3_BIT_KHR,
                                                    EGL_BLUE_SIZE,
                                                    8,
                                                    EGL_GREEN_SIZE,
                                                    8,
                                                    EGL_RED_SIZE,
                                                    8,
                                                    EGL_DEPTH_SIZE,
                                                    0,
                                                    EGL_STENCIL_SIZE,
                                                    0,
                                                    EGL_NONE};
static constexpr EGLint egl_pbuffer_attribs[5]{EGL_WIDTH,
                                               10,
                                               EGL_HEIGHT,
                                               10,
                                               EGL_NONE};
static constexpr std::array<EGLint, 5> egl_empty_attribs{EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
static constexpr std::array<EGLint, 4> egl_context_attribs{EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};

namespace eka2l1 {
    namespace drivers {

        emu_window_android::emu_window_android()
                : userdata(nullptr)
                , is_fullscreen_now(false) {

        }

        bool emu_window_android::get_mouse_button_hold(const int mouse_btt) {
            return false;
        }

        vec2 emu_window_android::window_size() {
            vec2 v;
            return v;
        }

        vec2 emu_window_android::window_fb_size() {
            vec2 v;
            return v;
        }

        vec2d emu_window_android::get_mouse_pos() {
            vec2d v;
            return v;
        }

        bool emu_window_android::set_cursor(cursor *cur) {
            return false;
        }
        void emu_window_android::cursor_visiblity(const bool visi) {

        }

        bool emu_window_android::cursor_visiblity() {
            return false;
        }

        bool emu_window_android::should_quit() {
            return false;
        }

        void emu_window_android::init(std::string title, vec2 size, const std::uint32_t flags) {

            emu_screen_size = size;
        }

        void emu_window_android::set_fullscreen(const bool is_fullscreen) {
            if (is_fullscreen_now == is_fullscreen) {
                return;
            }

            is_fullscreen_now = is_fullscreen;
        }

        void emu_window_android::change_title(std::string new_title) {

        }

        void emu_window_android::make_current() {
            eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
        }

        void emu_window_android::done_current() {

        }

        void emu_window_android::swap_buffer() {
            if (render_window) {
                eglSwapInterval(egl_display, 1);
                eglSwapBuffers(egl_display, egl_surface);
            }
        }

        void emu_window_android::shutdown() {

        }

        void emu_window_android::poll_events() {

        }

        void emu_window_android::set_userdata(void *new_userdata) {
            userdata = new_userdata;
        }

        void *emu_window_android::get_userdata() {
            return userdata;
        }

        void emu_window_android::surface_changed(ANativeWindow *surf, int width, int height) {
            render_window = surf;
            if (width > 0 && height > 0) {
                window_width = width;
                window_height = height;
            }
            if (inited) {
                host_window = surf;
            }
        }

        void emu_window_android::init_gl() {
            if (egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY); egl_display == EGL_NO_DISPLAY) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "eglGetDisplay() failed");
                return;
            }
            if (eglInitialize(egl_display, 0, 0) != EGL_TRUE) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "eglInitialize() failed");
                return;
            }
            if (EGLint egl_num_configs{}; eglChooseConfig(egl_display, egl_fake_attribs.data(), &egl_config, 1,
                                                          &egl_num_configs) != EGL_TRUE) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "eglChooseConfig() failed");
                return;
            }

            if (egl_surface = eglCreatePbufferSurface(egl_display, egl_config, egl_pbuffer_attribs);
                    egl_surface == EGL_NO_SURFACE) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "eglCreatePbufferSurface() failed");
                return;
            }

            if (egl_context = eglCreateContext(egl_display, egl_config, 0, egl_context_attribs.data());
                    egl_context == EGL_NO_CONTEXT) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "eglCreateContext() failed");
                return;
            }
            if (eglSurfaceAttrib(egl_display, egl_surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED) !=
                EGL_TRUE) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "eglSurfaceAttrib() failed");
                return;
            }
            if (eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context) != EGL_TRUE) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "eglMakeCurrent() failed");
                return;
            }
            if (!gladLoadGLES2Loader((GLADloadproc)eglGetProcAddress)) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "gladLoadGLES2Loader() failed");
                return;
            }
        }

        void emu_window_android::init_surface() {
            if (!render_window) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "surface is nullptr");
                return;
            }

            host_window = render_window;

            if (EGLint egl_num_configs{}; eglChooseConfig(egl_display, egl_attribs.data(), &egl_config, 1,
                                                          &egl_num_configs) != EGL_TRUE) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "eglChooseConfig() failed");
                return;
            }

            destroy_surface();

            create_surface();

            if (eglSurfaceAttrib(egl_display, egl_surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED) !=
                EGL_TRUE) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "eglSurfaceAttrib() failed");
                return;
            }
            if (eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context) != EGL_TRUE) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "eglMakeCurrent() failed");
                return;
            }
            inited = true;
        }

        void emu_window_android::create_surface() {
            if (!host_window) {
                return;
            }

            EGLint format;
            eglGetConfigAttrib(egl_display, egl_config, EGL_NATIVE_VISUAL_ID, &format);
            ANativeWindow_setBuffersGeometry(host_window, 0, 0, format);

            if (egl_surface = eglCreateWindowSurface(egl_display, egl_config, host_window, 0);
                    egl_surface == EGL_NO_SURFACE) {
                return;
            }
        }

        void emu_window_android::destroy_surface() {
            if (!egl_surface) {
                return;
            }
            if (eglGetCurrentSurface(EGL_DRAW) == egl_surface) {
                eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            }
            if (!eglDestroySurface(egl_display, egl_surface)) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "eglDestroySurface() failed");
            }
            egl_surface = EGL_NO_SURFACE;
        }
    }
}
