/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <core/services/applist/applist.h>
#include <core/services/applist/op.h>

#include <core/core.h>
#include <functional>

#include <e32lang.h>

namespace eka2l1 {
    applist_server::applist_server(system *sys)
        : service::server(sys, "!AppListServer") {
        REGISTER_IPC(applist_server, default_screen_number,
            EAppListServGetDefaultScreenNumber, "GetDefaultScreenNumber");
        REGISTER_IPC(applist_server, app_language,
            EAppListServApplicationLanguage, "ApplicationLanguageL");
    }

    /*! \brief Get the number of screen shared for an app. 
     * 
     * arg0: pointer to the number of screen
     * arg1: application UID
     * request_sts: KErrNotFound if app doesn't exist
    */
    void applist_server::default_screen_number(service::ipc_context ctx) {
        eka2l1::ptr<int> number = *ctx.get_arg<int>(0);
        system *tsys = ctx.sys;

        *number.get(tsys->get_memory_system()) = 1;
        ctx.set_request_status(0); // KErrNone
    }

    void applist_server::app_language(service::ipc_context ctx) {
        ctx.write_arg_pkg<TLanguage>(1, ELangEnglish);
        ctx.set_request_status(0);
    }
}