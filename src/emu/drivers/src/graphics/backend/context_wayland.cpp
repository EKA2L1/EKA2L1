// SPDX-FileCopyrightText: 2019-2022 Connor McLaughlin <stenzek@gmail.com>
// SPDX-License-Identifier: (GPL-3.0 OR CC-BY-NC-ND-4.0)

#include "context_wayland.h"
#include <common/log.h>
#include <dlfcn.h>

namespace eka2l1::drivers::graphics {
    static const char* WAYLAND_EGL_MODNAME = "libwayland-egl.so.1";

    gl_context_egl_wayland::gl_context_egl_wayland(const window_system_info& wsi, bool stereo, bool core)
        : gl_context_egl(wsi, stereo, core, false) {

    }

    gl_context_egl_wayland::~gl_context_egl_wayland() {
        if (render_window)
            wl_egl_window_destroy_(reinterpret_cast<wl_egl_window*>(render_window));
        if (wl_module_)
            dlclose(wl_module_);
    }

    void gl_context_egl_wayland::update(const std::uint32_t new_surface_width, const std::uint32_t new_surface_height) {
        if (!load_module()) {
            return;
        }

        if (render_window)
            wl_egl_window_resize_(reinterpret_cast<wl_egl_window*>(render_window), new_surface_width, new_surface_height, 0, 0);

        gl_context_egl::update(new_surface_width, new_surface_height);
    }

    void gl_context_egl_wayland::prepare_render_window() {
        if (!load_module()) {
            return;
        }

        if (render_window) {
            wl_egl_window_destroy_(reinterpret_cast<wl_egl_window*>(render_window));
            render_window = nullptr;
        }

        render_window = wl_egl_window_create_(static_cast<wl_surface*>(wsi.render_surface), wsi.surface_width, wsi.surface_height);
    }

    bool gl_context_egl_wayland::load_module() {
        if (try_loaded_) {
            return wl_module_ != nullptr;
        }

        try_loaded_ = true;
        wl_module_ = dlopen(WAYLAND_EGL_MODNAME, RTLD_NOW | RTLD_GLOBAL);

        if (!wl_module_) {
            LOG_ERROR(DRIVER_GRAPHICS, "Failed to load Wayland window library! GL context can't be created!");
            return false;
        }

        wl_egl_window_create_ =
            reinterpret_cast<decltype(wl_egl_window_create_)>(dlsym(wl_module_, "wl_egl_window_create"));
        wl_egl_window_destroy_ =
            reinterpret_cast<decltype(wl_egl_window_destroy_)>(dlsym(wl_module_, "wl_egl_window_destroy"));
        wl_egl_window_resize_ =
            reinterpret_cast<decltype(wl_egl_window_resize_)>(dlsym(wl_module_, "wl_egl_window_resize"));
        if (!wl_egl_window_create_ || !wl_egl_window_destroy_ || !wl_egl_window_resize_) {
            LOG_ERROR(DRIVER_GRAPHICS, "Failed to load one or more Wayland function!");
            return false;
        }

        return true;
    }
}