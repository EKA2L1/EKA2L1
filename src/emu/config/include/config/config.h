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

#include <config/options.inl>

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
    static constexpr const char *KEYBIND_DEFAULT_FILE = "keybind.yml";

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

    struct keybind_profile {
        std::vector<keybind> keybinds;

        void serialize(const std::string &file = KEYBIND_DEFAULT_FILE);
        void deserialize(const std::string &file = KEYBIND_DEFAULT_FILE);
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
        int emulator_language{ -1 };
        int audio_master_volume{ 100 };

        bool enable_gdbstub{ false };
        int gdb_port{ 24689 };

        std::string storage = "data"; // Set this to dot, avoid making it absolute

        bool enable_srv_backup{ true };
        bool enable_srv_rights{ true };
        bool enable_srv_sa{ true };
        bool enable_srv_drm{ true };
        bool enable_srv_akn_icon{ false };

        bool fbs_enable_compression_queue{ false };
        bool enable_btrace{ false };

        bool stop_warn_touch_disabled { false };
        bool dump_imb_range_code { false };
        bool hide_mouse_in_screen_space { false };
        bool nearest_neighbor_filtering { true };
        bool integer_scaling { true };
        bool cpu_load_save { true };
        bool mime_detection { true };

        std::atomic<bool> stepping { false };
        std::string rtos_level;

        bool ui_new_style { true };
        bool cenrep_reset { false };

        keybind_profile keybinds;

        std::string imei{ DEFAULT_IMI };
        std::string mmc_id{ DEFAULT_MMC_ID };
        std::string current_keybind_profile{ "default" };

        void serialize(const bool with_bindings = true);
        void deserialize(const bool with_bindings = true);
    };
}
