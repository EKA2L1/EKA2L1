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

#include <services/applist/applist.h>
#include <utils/apacmd.h>

namespace eka2l1::android {
    class launcher {
        eka2l1::system *sys;
        config::state *conf;

    public:
        explicit launcher(eka2l1::system *sys);

        std::vector<std::string> get_apps();
        void launch_app(std::uint32_t uid);
        bool install_app(std::string &path);
        std::vector<std::string> get_devices();
        void set_current_device(std::uint32_t id);
        std::uint32_t get_current_device();
        bool install_device(std::string &rpkg_path, std::string &rom_path, bool install_rpkg);
        std::vector<std::string> get_packages();
        void uninstall_package(std::uint32_t uid);
        void mount_sd_card(std::string &path);
        void load_config();
    };
}
