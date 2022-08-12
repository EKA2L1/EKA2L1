/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include <j2me/interface.h>
#include <j2me/kmidrun.h>
#include <j2me/applist.h>

#include <system/epoc.h>

namespace eka2l1::j2me {
    bool launch(system *sys, const std::uint32_t app_id) {
        app_list *applist = sys->get_j2me_applist();
        std::optional<app_entry> entry = applist->get_entry(app_id);
        if (!entry.has_value()) {
            return false;
        }
        const epocver sysver = sys->get_symbian_version_use();
        if (sysver <= epocver::epoc6) {
            launch_through_kmidrun(sys, entry.value());
            return true;
        }

        return false;
    }

    install_error install(system *sys, const std::string &path, app_entry &entry_info) {
        app_list *applist = sys->get_j2me_applist();
        const epocver sysver = sys->get_symbian_version_use();
        if (sysver <= epocver::epoc6) {
            return install_for_kmidrun(sys, applist, path, entry_info);
        }
        return INSTALL_ERROR_NOT_SUPPORTED_FOR_PLAT;
    }

    bool uninstall(system *sys, const std::uint32_t app_id) {
        app_list *applist = sys->get_j2me_applist();
        std::optional<app_entry> entry = applist->get_entry(app_id);
        if (!entry.has_value()) {
            return false;
        }
        const epocver sysver = sys->get_symbian_version_use();
        if (sysver <= epocver::epoc6) {
            uninstall_for_kmidrun(sys, applist, entry.value());
            return true;
        }
        return false;
    }
}