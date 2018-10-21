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

#include <core/services/ui/oom_app.h>
#include <common/log.h>

namespace eka2l1 {
    oom_ui_app_server::oom_ui_app_server(eka2l1::system *sys) 
        : service::server(sys, "101fdfae_10207218_AppServer", true) {
        REGISTER_IPC(oom_ui_app_server, add_to_menu_list, 0x42, "OOM::AddToMenuList42");
        REGISTER_IPC(oom_ui_app_server, add_to_menu_list2, 0x43, "OOM::AddToMenuList43");
    }

    void oom_ui_app_server::add_to_menu_list(service::ipc_context ctx) {
        LOG_TRACE("OOM 0x42 stubbed");
        ctx.set_request_status(0);
    }

    void oom_ui_app_server::add_to_menu_list2(service::ipc_context ctx) {
        LOG_TRACE("OOM 0x43 stubbed");
        ctx.set_request_status(0);
    }
}