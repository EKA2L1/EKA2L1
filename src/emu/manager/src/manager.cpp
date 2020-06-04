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

#include <manager/device_manager.h>
#include <manager/manager.h>
#include <manager/package_manager.h>

#ifdef ENABLE_SCRIPTING
#include <manager/script_manager.h>
#endif

#include <epoc/epoc.h>

namespace eka2l1 {
    manager::package_manager *manager_system::get_package_manager() {
        return pkgmngr.get();
    }

#ifdef ENABLE_SCRIPTING
    manager::script_manager *manager_system::get_script_manager() {
        return scrmngr.get();
    }
#endif

    manager::device_manager *manager_system::get_device_manager() {
        return dvmngr.get();
    }

    void manager_system::init(system *sys, config::state *conf) {
        pkgmngr = std::make_unique<manager::package_manager>(sys, conf);

#ifdef ENABLE_SCRIPTING
        scrmngr = std::make_unique<manager::script_manager>(sys);
#endif
        dvmngr = std::make_unique<manager::device_manager>(conf);
    }
}
