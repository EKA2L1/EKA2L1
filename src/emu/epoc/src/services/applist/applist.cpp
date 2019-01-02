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

#include <epoc/services/applist/applist.h>
#include <epoc/services/applist/op.h>

#include <common/cvt.h>
#include <common/log.h>

#include <epoc/epoc.h>
#include <functional>

#include <e32lang.h>

namespace eka2l1 {
    applist_server::applist_server(system *sys)
        : service::server(sys, "!AppListServer", true) {
        REGISTER_IPC(applist_server, default_screen_number,
            EAppListServGetDefaultScreenNumber, "GetDefaultScreenNumber");
        REGISTER_IPC(applist_server, app_language,
            EAppListServApplicationLanguage, "ApplicationLanguageL");
        REGISTER_IPC(applist_server, is_accepted_to_run,
            EAppListServRuleBasedLaunching, "RuleBasedLaunching");
    }

    void applist_server::is_accepted_to_run(service::ipc_context ctx) {
        auto exe_name = ctx.get_arg<std::u16string>(0);

        if (!exe_name) {
            ctx.set_request_status(false);
            return;
        }

        LOG_TRACE("Asking permission to launch: {}, accepted", common::ucs2_to_utf8(*exe_name));
        ctx.set_request_status(true);
    }

    /*! \brief Get the number of screen shared for an app. 
     * 
     * arg0: pointer to the number of screen
     * arg1: application UID
     * request_sts: KErrNotFound if app doesn't exist
    */
    void applist_server::default_screen_number(service::ipc_context ctx) {
        // TODO: Detect if app exists. Sanity check
        LOG_TRACE("DefaultScreenNumber stubbed with 0");
        ctx.set_request_status(0); // KErrNone
    }

    void applist_server::app_language(service::ipc_context ctx) {
        ctx.write_arg_pkg<TLanguage>(1, ELangEnglish);
        ctx.set_request_status(0);
    }
}