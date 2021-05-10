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

#include <system/installation/common.h>
#include <services/applist/applist.h>
#include <services/window/window.h>
#include <utils/apacmd.h>

namespace eka2l1 {
    class fbs_server;
}

namespace eka2l1::android {
    class launcher {
        eka2l1::system *sys;
        config::state *conf;
        eka2l1::kernel_system *kern;
        applist_server *alserv;
        window_server *winserv;
        fbs_server *fbsserv;

        void set_language_to_property(const language new_one);
        void set_language_current(const language lang);

    public:
        explicit launcher(eka2l1::system *sys);

        std::vector<std::string> get_apps();
        std::optional<apa_app_masked_icon_bitmap> get_app_icon(std::uint32_t uid);
        void launch_app(std::uint32_t uid);
        bool install_app(std::string &path);
        std::vector<std::string> get_devices();
        void set_current_device(std::uint32_t id);
        void set_device_name(std::uint32_t id, const char *name);
        std::uint32_t get_current_device();
        device_installation_error install_device(std::string &rpkg_path, std::string &rom_path, bool install_rpkg);
        bool does_rom_need_rpkg(const std::string &rom_path);
        std::vector<std::string> get_packages();
        void uninstall_package(std::uint32_t uid);
        void mount_sd_card(std::string &path);
        void load_config();
        void set_language(std::uint32_t language_id);
        void set_rtos_level(std::uint32_t level);
        void update_app_setting(std::uint32_t uid);
        void draw(drivers::graphics_command_list_builder *builder, std::uint32_t width, std::uint32_t height);

        fbs_server *get_fbs_serv() {
            return fbsserv;
        }
    };
}
