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
    static constexpr std::array<EGLint, 13> egl_attribs{ EGL_SURFACE_TYPE,
                                                         EGL_WINDOW_BIT,
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
    static constexpr std::array<EGLint, 15> egl_attribs_es{ EGL_SURFACE_TYPE,
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
    static constexpr std::array<EGLint, 15> egl_fake_attribs_es{ EGL_SURFACE_TYPE,
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
    static constexpr std::array<EGLint, 13> egl_fake_attribs{ EGL_SURFACE_TYPE,
                                                              EGL_PBUFFER_BIT,
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
    static constexpr std::array<EGLint, 3> egl_context_attribs_es{ EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };

    bool gl_context_egl::is_headless() const {
        return !render_window;
    }

    bool gl_context_egl::create_context(EGLContext shared, std::pair<int, int> *target_version) {
        if (m_opengl_mode == mode::opengl_es) {
            if (egl_context = eglCreateContext(egl_display, egl_config, shared, egl_context_attribs_es.data());
                    egl_context == EGL_NO_CONTEXT) {
                LOG_CRITICAL(DRIVER_GRAPHICS, "eglCreateContext() failed");
                return false;
            }
        } else {
            std::array<EGLint, 5> egl_context_attrib_template{ EGL_CONTEXT_MAJOR_VERSION, 0,
                EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE };
            
            if (target_version != nullptr) {
                egl_context_attrib_template[1] = target_version->first;
                egl_context_attrib_template[3] = target_version->second;

                if (egl_context = eglCreateContext(egl_display, egl_config, shared, egl_context_attrib_template.data());
                        egl_context == EGL_NO_CONTEXT) {
                    LOG_CRITICAL(DRIVER_GRAPHICS, "eglCreateContext() failed");
                    return false;
                }

                context_version = *target_version;
            } else {
                for (auto version: s_desktop_opengl_versions) {
                    egl_context_attrib_template[1] = version.first;
                    egl_context_attrib_template[3] = version.second;

                    if (egl_context = eglCreateContext(egl_display, egl_config, shared, egl_context_attrib_template.data());
                            egl_context != EGL_NO_CONTEXT) {
                        context_version = version;
                        break;
                    }
                }

                if (egl_context == EGL_NO_CONTEXT) {
                    LOG_CRITICAL(DRIVER_GRAPHICS, "eglCreateContext() failed");
                    return false;
                }
            }
        }

        return true;
    }

    void gl_context_egl::init_gl() {
        if (egl_display = eglGetDisplay(wsi.display_connection ? reinterpret_cast<EGLNativeDisplayType>(wsi.display_connection) : EGL_DEFAULT_DISPLAY); egl_display == EGL_NO_DISPLAY) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglGetDisplay() failed");
            return;
        }
        if (eglInitialize(egl_display, 0, 0) != EGL_TRUE) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "eglInitialize() failed");
            return;
        }

        if (eglBindAPI((m_opengl_mode == mode::opengl_es) ? EGL_OPENGL_ES_API : EGL_OPENGL_API) != EGL_TRUE) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "Can't bind target graphics API!");
            return;
        }

        if (!create_context(nullptr, nullptr)) {
            return;
        }
    }

    void gl_context_egl::init_surface() {
        prepare_render_window();

        if (!render_window) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "surface is nullptr");
            return;
        }

        if (EGLint egl_num_configs{}; eglChooseConfig(egl_display,
            (m_opengl_mode == mode::opengl_es) ? egl_attribs_es.data() : egl_attribs.data(), &egl_config, 1,
            &egl_num_configs) != EGL_TRUE) {
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
        render_window = new_surface;

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

    gl_context_egl::gl_context_egl(const window_system_info& wsi, bool stereo, bool core, bool is_gles)
        : egl_surface(nullptr)
        , egl_context(nullptr)
        , egl_display(nullptr)
        , render_window(nullptr)
        , wsi(wsi) {
        m_opengl_mode = is_gles ? mode::opengl_es : mode::opengl;
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

    void gl_context_egl::create_surface() {
        if (!render_window) {
            return;
        }

        if (egl_surface = eglCreateWindowSurface(egl_display, egl_config, reinterpret_cast<EGLNativeWindowType>(render_window), 0);
            egl_surface == EGL_NO_SURFACE) {
            LOG_ERROR(DRIVER_GRAPHICS, "Create window surface failed with code: {}", eglGetError());
            return;
        }

        EGLint surface_width = 1;
        EGLint surface_height = 1;

        if (!eglQuerySurface(egl_display, egl_surface, EGL_WIDTH, &surface_width) ||
            !eglQuerySurface(egl_display, egl_surface, EGL_HEIGHT, &surface_height))  {
            LOG_ERROR(DRIVER_GRAPHICS, "Failed to retrieve surface width or height.");
            return;
        }

        m_backbuffer_width = static_cast<int>(surface_width);
        m_backbuffer_height = static_cast<int>(surface_height);
    }

    void gl_context_egl::destroy_surface() {
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

    bool gl_context_egl::init_gl_for_shared(gl_context_egl *parent) {
        egl_display = parent->egl_display;
        egl_config = parent->egl_config;

        m_opengl_mode = parent->m_opengl_mode;

        if (eglBindAPI((m_opengl_mode == mode::opengl_es) ? EGL_OPENGL_ES_API : EGL_OPENGL_API) != EGL_TRUE) {
            LOG_CRITICAL(DRIVER_GRAPHICS, "Can't bind target graphics API!");
            return false;
        }

        if (!create_context(parent->egl_context, &parent->context_version)) {
            return false;
        }

        return true;
    }

    std::unique_ptr<gl_context> gl_context_egl::create_shared_context() {
        std::unique_ptr<gl_context_egl> shared_context = std::make_unique<gl_context_egl>();
        if (!shared_context->init_gl_for_shared(this)) {
            return nullptr;
        }

        return shared_context;
    }

    void gl_context_egl::update(const std::uint32_t new_width, const std::uint32_t new_height) {
        m_backbuffer_width = static_cast<int>(new_width);
        m_backbuffer_height = static_cast<int>(new_height);
    }
}