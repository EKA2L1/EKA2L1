// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <common/vecx.h>
#include <drivers/graphics/emu_window.h>

#include <atomic>
#include <cstdint>

namespace eka2l1::drivers {
    // iOS-side analogue of emu_window_android: holds the CAEAGLLayer pointer
    // owned by the frontend's EAGLView and the current framebuffer geometry,
    // so window_system_info::render_surface can be threaded into the EAGL
    // graphics context. The frontend updates dimensions from
    // viewDidLayoutSubviews; pointer events bypass emu_window and go through
    // IosEmulator directly.
    class emu_window_ios final : public emu_window {
    public:
        emu_window_ios();

        // Frontend hooks ----------------------------------------------------
        // Called by EAGLView whenever the bound CAEAGLLayer is created or
        // recreated. `layer` is a CAEAGLLayer* (kept opaque so this header
        // stays pure C++).
        void surface_changed(void *layer, int pixel_width, int pixel_height, float scale);

        // emu_window interface ---------------------------------------------
        void init(std::string title, vec2 size, const std::uint32_t flags) override;
        void poll_events() override;
        void shutdown() override;
        void set_fullscreen(const bool is_fullscreen) override;
        bool should_quit() override;
        void change_title(std::string new_title) override;

        vec2 window_size() override;
        vec2 window_fb_size() override;
        vec2d get_mouse_pos() override;
        bool get_mouse_button_hold(const int mouse_btt) override;

        void set_userdata(void *userdata) override;
        void *get_userdata() override;

        bool set_cursor(cursor *cur) override;
        void cursor_visiblity(const bool visi) override;
        bool cursor_visiblity() override;

        window_system_info get_window_system_info() override;

    private:
        void *userdata_ = nullptr;
        void *layer_ = nullptr;
        vec2 fb_size_ = vec2(0, 0);
        float scale_ = 1.0f;
        std::atomic<bool> should_quit_{false};
    };
}
