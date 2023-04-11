// SPDX-FileCopyrightText: 2019-2022 Connor McLaughlin <stenzek@gmail.com>
// SPDX-License-Identifier: (GPL-3.0 OR CC-BY-NC-ND-4.0)

#pragma once

#include <wayland-egl.h>
#include "context_egl.h"

namespace eka2l1::drivers::graphics {
    class gl_context_egl_wayland final : public gl_context_egl {
    public:
        explicit gl_context_egl_wayland() = default;
        explicit gl_context_egl_wayland(const window_system_info& wsi, bool stereo, bool core);

        ~gl_context_egl_wayland() override;

        void update(const std::uint32_t new_width, const std::uint32_t new_height) override;

    protected:
        void prepare_render_window() override;

    private:
        bool load_module();

        bool try_loaded_ = false;

        void* wl_module_ = nullptr;
        wl_egl_window* (*wl_egl_window_create_)(struct wl_surface* surface, int width, int height);
        void (*wl_egl_window_destroy_)(struct wl_egl_window* egl_window);
        void (*wl_egl_window_resize_)(struct wl_egl_window* egl_window, int width, int height, int dx, int dy);
    };
}