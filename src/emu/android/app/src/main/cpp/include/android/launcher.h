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

#pragma once

#include <android/state.h>

#include <common/cvt.h>
#include <common/types.h>

#include <drivers/ui/input_dialog.h>
#include <services/applist/applist.h>
#include <services/window/window.h>
#include <system/installation/common.h>
#include <utils/apacmd.h>

namespace eka2l1 {
    class fbs_server;

    namespace epoc {
        struct screen;
    }
}

namespace eka2l1::android {
    class launcher {
        eka2l1::system *sys;
        config::state *conf;
        eka2l1::kernel_system *kern;
        applist_server *alserv;
        window_server *winserv;
        fbs_server *fbsserv;

        eka2l1::vecx<std::uint8_t, 3> background_color_;
        float scale_ratio_;
        std::uint32_t scale_type_;
        std::uint32_t gravity_;
        eka2l1::drivers::ui::input_dialog_complete_callback input_complete_callback_;

        void set_language_to_property(const language new_one);
        void set_language_current(const language lang);
        void retrieve_servers();

    public:
        explicit launcher(eka2l1::system *sys);

        std::vector<std::string> get_apps();
        jobjectArray get_app_icon(JNIEnv *env, std::uint32_t uid);
        void launch_app(std::uint32_t uid);
        package::installation_result install_app(std::string &path);
        std::vector<std::string> get_devices();
        std::vector<std::string> get_device_firwmare_codes();
        void set_current_device(std::uint32_t id, const bool temporary);
        void set_device_name(std::uint32_t id, const char *name);
        void rescan_devices();
        std::uint32_t get_current_device();
        device_installation_error install_device(std::string &rpkg_path, std::string &rom_path, bool install_rpkg);
        bool does_rom_need_rpkg(const std::string &rom_path);
        std::vector<std::string> get_packages();
        void uninstall_package(std::uint32_t uid, std::int32_t ext_index);
        void mount_sd_card(std::string &path);
        void load_config();
        void set_language(std::uint32_t language_id);
        void set_rtos_level(std::uint32_t level);
        void update_app_setting(std::uint32_t uid);
        void draw(drivers::graphics_command_builder &builder, epoc::screen *scr,
                  std::uint32_t width, std::uint32_t height);
        std::vector<std::string> get_language_ids();
        std::vector<std::string> get_language_names();
        void set_screen_params(std::uint32_t background_color, std::uint32_t scale_ratio,
                               std::uint32_t scale_type, std::uint32_t gravity);
        bool open_input_view(const std::u16string &initial_text,
                             const int max_len,
                             drivers::ui::input_dialog_complete_callback complete_callback);
        void close_input_view();
        void on_finished_text_input(const std::string &text, const bool force_close);
        int install_ngage_game(const std::string &path);

        fbs_server *get_fbs_serv() {
            return fbsserv;
        }
    };
}
