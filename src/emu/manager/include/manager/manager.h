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

#include <epoc/configure.h>
#include <manager/package_manager.h>

#include <memory>

namespace eka2l1 {
    class system;
}

namespace eka2l1 {
    #ifdef ENABLE_SCRIPTING
    namespace manager {
        class script_manager;
    }
    #endif

    class manager_system {
        #ifdef ENABLE_SCRIPTING
        std::shared_ptr<manager::script_manager> scrmngr;
        #endif

        manager::package_manager pkgmngr;

        io_system *io;

    public:
        void init(system *sys, io_system *ios);

        manager::package_manager *get_package_manager();

        #ifdef ENABLE_SCRIPTING
        manager::script_manager *get_script_manager();
        #endif
    };
}