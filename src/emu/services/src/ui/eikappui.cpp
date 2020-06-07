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

#include <common/log.h>
#include <epoc/epoc.h>
#include <services/ui/eikappui.h>
#include <utils/err.h>

#include <cstring>

namespace eka2l1 {
    std::string debug_preferences::to_buf() {
        // Write a dummy version
        std::string buf;
        buf.resize(3);

        std::uint16_t eik_ver = 0;
        std::uint8_t flags8 = static_cast<std::uint8_t>(flags);

        std::memcpy(&buf[0], &eik_ver, sizeof(eik_ver));
        std::memcpy(&buf[sizeof(eik_ver)], &flags8, sizeof(flags8));

        return buf;
    }

    eikappui_server::eikappui_server(eka2l1::system *sys)
        : service::server(sys->get_kernel_system(), sys, "!EikAppUiServer", true) {
        REGISTER_IPC(eikappui_server, get_debug_preferences, EEikAppUiGetDebugPreferences, "EikAppUi::GetDebugPreferences");
    }

    // TODO: Make a resource reader and read from the config resource file
    void eikappui_server::get_debug_preferences(service::ipc_context &ctx) {
        debug_preferences preferences;
        LOG_TRACE("GetDebugPreferences stubbed");

        std::string buf = preferences.to_buf();

        // Buffer is not large enough. Unless the length is specified as 0, we are still doing this check
        if ((*ctx.get_arg<int>(0) != 0) && (static_cast<int>(buf.size()) > *ctx.get_arg<int>(0))) {
            ctx.set_request_status(static_cast<int>(buf.size()));
            return;
        }

        ctx.write_arg_pkg(1, reinterpret_cast<std::uint8_t *>(&buf[0]),
            static_cast<std::uint32_t>(buf.size()));

        ctx.set_request_status(0);
    }
}
