// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <drivers/graphics/backend/emu_window_ios.h>

namespace eka2l1::drivers {
    emu_window_ios::emu_window_ios() = default;

    void emu_window_ios::surface_changed(void *layer, int pixel_width, int pixel_height, float scale) {
        layer_ = layer;
        if (pixel_width > 0 && pixel_height > 0) {
            fb_size_.x = pixel_width;
            fb_size_.y = pixel_height;
        }
        if (scale > 0.0f) {
            scale_ = scale;
        }
        if (surface_change_hook) {
            surface_change_hook(layer_);
        }
        if (resize_hook) {
            resize_hook(userdata_, fb_size_);
        }
    }

    void emu_window_ios::init(std::string /*title*/, vec2 /*size*/, const std::uint32_t /*flags*/) {}
    void emu_window_ios::poll_events() {}
    void emu_window_ios::shutdown() { should_quit_ = true; }
    void emu_window_ios::set_fullscreen(const bool /*is_fullscreen*/) {}
    bool emu_window_ios::should_quit() { return should_quit_.load(); }
    void emu_window_ios::change_title(std::string /*new_title*/) {}

    vec2 emu_window_ios::window_size() {
        if (scale_ <= 0.0f) {
            return fb_size_;
        }
        return vec2(static_cast<int>(fb_size_.x / scale_), static_cast<int>(fb_size_.y / scale_));
    }

    vec2 emu_window_ios::window_fb_size() { return fb_size_; }
    vec2d emu_window_ios::get_mouse_pos() { return vec2d{ 0.0, 0.0 }; }
    bool emu_window_ios::get_mouse_button_hold(const int /*mouse_btt*/) { return false; }

    void emu_window_ios::set_userdata(void *userdata) { userdata_ = userdata; }
    void *emu_window_ios::get_userdata() { return userdata_; }

    bool emu_window_ios::set_cursor(cursor * /*cur*/) { return false; }
    void emu_window_ios::cursor_visiblity(const bool /*visi*/) {}
    bool emu_window_ios::cursor_visiblity() { return false; }

    window_system_info emu_window_ios::get_window_system_info() {
        window_system_info info;
        info.type = window_system_type::iOS;
        info.render_surface = layer_;
        info.render_window = layer_;
        info.render_surface_scale = scale_;
        info.surface_width = static_cast<std::uint32_t>(fb_size_.x);
        info.surface_height = static_cast<std::uint32_t>(fb_size_.y);
        return info;
    }
}
