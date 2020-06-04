/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <common/configure.h>
#include <manager/package_manager.h>

#include <memory>

namespace eka2l1 {
    class system;
}

namespace eka2l1 {
    namespace config {
        struct state;
    }

    namespace manager {
        class device_manager;
        class config_manager;

#ifdef ENABLE_SCRIPTING
        class script_manager;
#endif
    }

    class manager_system {
#ifdef ENABLE_SCRIPTING
        std::unique_ptr<manager::script_manager> scrmngr;
#endif
        std::unique_ptr<manager::device_manager> dvmngr;
        std::unique_ptr<manager::package_manager> pkgmngr;

    public:
        manager_system() = default;
        ~manager_system() = default;

        void init(system *sys, config::state *conf);

        manager::package_manager *get_package_manager();

#ifdef ENABLE_SCRIPTING
        manager::script_manager *get_script_manager();
#endif

        manager::device_manager *get_device_manager();

        manager::config_manager *get_config_manager();
    };
}
