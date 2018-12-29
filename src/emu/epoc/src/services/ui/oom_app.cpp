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

#include <epoc/services/ui/oom_app.h>
#include <common/log.h>

#include <common/e32inc.h>
#include <e32err.h>

namespace eka2l1 {
    oom_ui_app_server::oom_ui_app_server(eka2l1::system *sys) 
        : service::server(sys, "101fdfae_10207218_AppServer", true) {
        REGISTER_IPC(oom_ui_app_server, get_layout_config_size, EAknEikAppUiLayoutConfigSize, "OOM::GetLayoutConfigSize");
        REGISTER_IPC(oom_ui_app_server, get_layout_config, EAknEikAppUiGetLayoutConfig, "OOM::GetLayoutConfig");
    }

    void oom_ui_app_server::get_layout_config_size(service::ipc_context ctx) {
        LOG_TRACE("OOM: GETLAYOUTCONFIGSIZE: Null descriptor on hardware");

        int size = 0;

        ctx.write_arg_pkg<int>(0, size);
        ctx.set_request_status(KErrNone);
    }

    void oom_ui_app_server::get_layout_config(service::ipc_context ctx) {
        LOG_TRACE("OOM: GETLAYOUTCONFIG: Null descriptor on hardware");
        
        ctx.write_arg_pkg(0, nullptr, 0);
        ctx.set_request_status(KErrNone);
    }
}