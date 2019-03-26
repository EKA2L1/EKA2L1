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

#include <manager/manager.h>
#include <manager/package_manager.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>

#include <epoc/epoc.h>
#include <functional>

#include <common/e32inc.h>
#include <e32err.h>
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
        REGISTER_IPC(applist_server, get_app_info,
            EAppListServGetAppInfo, "GetAppInfo");
        REGISTER_IPC(applist_server, get_capability,
            EAppListServGetAppCapability, "GetAppCapability")
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

    void applist_server::default_screen_number(service::ipc_context ctx) {
        // TODO: Detect if app exists. Sanity check
        LOG_TRACE("DefaultScreenNumber stubbed with 0");
        ctx.set_request_status(0); // KErrNone
    }

    void applist_server::app_language(service::ipc_context ctx) {
        LOG_TRACE("AppList::AppLanguage stubbed to returns ELangEnglish");

        TLanguage default_lang = ELangEnglish;

        ctx.write_arg_pkg<TLanguage>(1, default_lang);
        ctx.set_request_status(0);
    }

    void applist_server::get_app_info(service::ipc_context ctx) {
        manager::package_manager *pkg_mngr = ctx.sys->get_manager_system()->get_package_manager();
        std::optional<manager::app_info> info = pkg_mngr->info(static_cast<std::uint32_t>(*ctx.get_arg<int>(0)));

        if (!info) {
            ctx.set_request_status(KErrNotFound);
            return;
        }

        std::u16string app_to_run_path;
        app_to_run_path += drive_to_char16(info->drive);

        // Craft some fake app path
        if (ctx.sys->get_symbian_version_use() >= epocver::epoc93) {
            app_to_run_path += u":\\sys\\bin\\";
        } else {
            app_to_run_path += u":\\system\\programs\\";
        }

        app_to_run_path += info->executable_name;

        apa_app_info apa_info;
        apa_info.uid = info->id;
        apa_info.app_path = app_to_run_path;
        apa_info.short_caption = info->name;
        apa_info.long_caption = info->name;

        ctx.write_arg_pkg<apa_app_info>(1, apa_info);
        ctx.set_request_status(KErrNone);
    }

    void applist_server::get_capability(service::ipc_context ctx) {
        std::uint32_t app_uid = *ctx.get_arg<int>(1);

        LOG_TRACE("GetCapability stubbed");

        apa_capability cap;
        cap.ability = apa_capability::embeddability::embeddable;
        cap.support_being_asked_to_create_new_file = false;
        cap.is_hidden = false;
        cap.launch_in_background = false;
        cap.group_name = u"gamers";

        if (ctx.sys->get_symbian_version_use() < epocver::epoc93) {
            // EKA1, we should check if app is DLL
            // TODO: more proper way to check
            manager::package_manager *pkg_mngr = ctx.sys->get_manager_system()->get_package_manager();
            std::string app_path = pkg_mngr->get_app_executable_path(app_uid);

            if (eka2l1::path_extension(app_path) == ".app") {
                cap.flags |= apa_capability::built_as_dll;
                cap.flags |= apa_capability::non_native;
            }
        }

        ctx.write_arg_pkg<apa_capability>(0, cap);
        ctx.set_request_status(KErrNone);
    }
}
