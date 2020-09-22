/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace eka2l1::config {
    static constexpr const char *KEYBIND_TYPE_KEY = "key";
    static constexpr const char *KEYBIND_TYPE_CONTROLLER = "controller";
    static constexpr const char *KEYBIND_TYPE_MOUSE = "mouse";

    struct keybind {
        struct {
            std::string type; // one of "key", "controller"
            union {
                std::uint32_t keycode;
                struct {
                    int controller_id;
                    int button_id;
                } button;
            } data;
        } source;
        std::uint32_t target; // target keycode
    };

    struct state {
        float menu_height = 0;
        int bkg_transparency{ 129 };
        float ui_scale{ 1.0 };
        std::string bkg_path;
        std::string font_path;

        bool log_read{ false };
        bool log_write{ false };
        bool log_svc{ false };
        bool log_ipc{ false };
        bool log_passed{ false };
        bool log_exports{ false };

        std::string cpu_backend{ "dynarmic" };
        int device{ 0 };
        int language{ -1 };

        bool enable_gdbstub{ false };
        int gdb_port{ 24689 };

        std::string storage = "data"; // Set this to dot, avoid making it absolute

        bool enable_srv_ecom{ true };
        bool enable_srv_cenrep{ true };
        bool enable_srv_backup{ true };
        bool enable_srv_install{ true };
        bool enable_srv_rights{ true };
        bool enable_srv_sa{ true };
        bool enable_srv_drm{ true };
        bool enable_srv_eikapp_ui{ true };
        bool enable_srv_akn_icon{ false };
        bool enable_srv_akn_skin{ true };
        bool enable_srv_cdl{ true };
        bool enable_srv_socket{ true };

        bool fbs_enable_compression_queue{ false };
        bool accurate_ipc_timing{ false };
        bool enable_btrace{ false };

        bool stop_warn_touch_disabled { false };
        bool dump_imb_range_code { false };
        bool hide_mouse_in_screen_space { false };

        std::vector<keybind> keybinds;
        std::atomic<std::uint16_t> time_getter_sleep_us { 0 };

        void serialize();
        void deserialize();
    };
}
