/*
 * Copyright (c) 2021 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
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

#include "context_egl.h"
#include <common/log.h>

namespace eka2l1::drivers::graphics {
    static constexpr std::array<EGLint, 15> egl_attribs{ EGL_SURFACE_TYPE,
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
                                                         EGL_NONE };
    static constexpr std::array<EGLint, 15> egl_fake_attribs{ EGL_SURFACE_TYPE,
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
                                                              EGL_NONE };
    static constexpr EGLint egl_pbuffer_attribs[5]{ EGL_WIDTH,
                                                    10,
                                                    EGL_HEIGHT,
                                                    10,
                                                    EGL_NONE };
    static constexpr std::array<EGLint, 5> egl_empty_attribs{ EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    static constexpr std::array<EGLint, 4> egl_context_attribs{ EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };

    bool gl_context_egl::is_headless() const {
        return !render_window;
    }

    void gl_context_egl::init_gl() {
        if (egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY); egl_display == EGL_NO_DISPLAY) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglGetDisplay() failed");
            return;
        }
        if (eglInitialize(egl_display, 0, 0) != EGL_TRUE) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglInitialize() failed");
            return;
        }
        if (EGLint egl_num_configs{}; eglChooseConfig(egl_display, egl_fake_attribs.data(), &egl_config, 1,
                                                      &egl_num_configs)
                                      != EGL_TRUE) {
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
        if (eglSurfaceAttrib(egl_display, egl_surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED) != EGL_TRUE) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglSurfaceAttrib() failed");
            return;
        }

        m_opengl_mode = mode::opengl_es;

        if (eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context) != EGL_TRUE) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglMakeCurrent() failed");
            return;
        }
    }

    void gl_context_egl::init_surface() {
        if (!render_window) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "surface is nullptr");
            return;
        }

        if (EGLint egl_num_configs{}; eglChooseConfig(egl_display, egl_attribs.data(), &egl_config, 1,
                                                      &egl_num_configs)
                                      != EGL_TRUE) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglChooseConfig() failed");
            return;
        }

        destroy_surface();
        create_surface();

        if (eglSurfaceAttrib(egl_display, egl_surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED) != EGL_TRUE) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglSurfaceAttrib() failed");
            return;
        }
        if (eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context) != EGL_TRUE) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglMakeCurrent() failed");
            return;
        }
    }

    void gl_context_egl::update_surface(void *new_surface) {
        render_window = reinterpret_cast<ANativeWindow*>(new_surface);

        clear_current();
        destroy_surface();
        create_surface();
        make_current();
    }

    bool gl_context_egl::make_current() {
        if (!egl_surface) {
            init_surface();
            return true;
        }
        return eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
    }

    bool gl_context_egl::clear_current() {
        return eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    void gl_context_egl::swap_buffers() {
        if (render_window) {
            eglSwapBuffers(egl_display, egl_surface);
        }
    }

    void gl_context_egl::set_swap_interval(const std::int32_t interval) {
        if (render_window) {
            eglSwapInterval(egl_display, interval);
        }
    }

    gl_context_egl::gl_context_egl(const window_system_info& wsi, bool stereo, bool core)
        : egl_surface(nullptr)
        , egl_context(nullptr)
        , egl_display(nullptr)
        , render_window(nullptr) {
        init_gl();
    }

    gl_context_egl::~gl_context_egl() {
        destroy_surface();

        if (egl_context) {
            if (eglGetCurrentContext() == egl_context) {
                clear_current();
            }

            if (!eglDestroyContext(egl_display, egl_context)) {
                LOG_ERROR(DRIVER_GRAPHICS, "Failed to destroy GLES context!");
            }
        }
    }

    std::unique_ptr<gl_context> gl_context_egl::create_shared_context() {
        LOG_ERROR(DRIVER_GRAPHICS, "Shared context is not supported on Android yet!");
        return nullptr;
    }

    void gl_context_egl::update() {

    }
}