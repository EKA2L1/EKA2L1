/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <epoc/services/window/classes/scrdvc.h>
#include <epoc/services/window/window.h>

#include <epoc/services/window/classes/winbase.h>
#include <epoc/services/window/classes/wingroup.h>

#include <epoc/services/window/op.h>
#include <epoc/services/window/opheader.h>

#include <common/e32inc.h>
#include <e32err.h>

namespace eka2l1::epoc {
    screen_device::screen_device(window_server_client_ptr client, epoc::screen *scr)
        : window_client_obj(client, scr) {
    }

    void screen_device::set_screen_mode_and_rotation(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        pixel_twips_and_rot *info = reinterpret_cast<decltype(info)>(cmd.data_ptr);

        for (int i = 0; i < scr->scr_config.modes.size(); i++) {
            epoc::config::screen_mode &mode =  scr->scr_config.modes[i];

            if (mode.size == info->pixel_size &&  number_to_orientation(mode.rotation) == info->orientation) {
                // Eureka... Bắt được mày rồi....
                scr->set_screen_mode(client->get_ws().get_graphics_driver(), mode.mode_number);
                ctx.set_request_status(KErrNone);
                return;
            }
        }

        LOG_ERROR("Unable to set size and orientation: mode not found!");
        ctx.set_request_status(KErrNotSupported);
    }

    void screen_device::set_screen_mode(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        const int mode = *reinterpret_cast<int *>(cmd.data_ptr);
        scr->set_screen_mode(client->get_ws().get_graphics_driver(), mode);
    }

    void screen_device::get_screen_size_mode_list(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        std::vector<int> modes;

        for (int i = 0; i < scr->scr_config.modes.size(); i++) {
            modes.push_back(scr->scr_config.modes[i].mode_number);
        }

        ctx.write_arg_pkg(reply_slot, reinterpret_cast<std::uint8_t *>(&modes[0]),
            static_cast<std::uint32_t>(sizeof(int) * modes.size()));

        ctx.set_request_status(scr->total_screen_mode());
    }

    void screen_device::get_screen_size_mode_and_rotation(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd,
        const bool bonus_the_twips) {
        const int mode = *reinterpret_cast<int *>(cmd.data_ptr);
        const epoc::config::screen_mode *scr_mode = scr->mode_info(mode);

        if (!scr_mode) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        if (bonus_the_twips) {
            pixel_twips_and_rot data;
            data.pixel_size = scr_mode->size;
            data.twips_size = scr_mode->size * twips_mul;
            data.orientation = number_to_orientation(scr_mode->rotation);

            ctx.write_arg_pkg(reply_slot, data);
        } else {
            pixel_and_rot data;
            data.pixel_size = scr_mode->size;
            data.orientation = number_to_orientation(scr_mode->rotation);
            
            ctx.write_arg_pkg(reply_slot, data);
        }

        ctx.set_request_status(KErrNone);
    }
            
    void screen_device::execute_command(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        TWsScreenDeviceOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsSdOpPixelSize: {
            // This doesn't take any arguments
            ctx.write_arg_pkg<eka2l1::vec2>(reply_slot, scr->size());
            ctx.set_request_status(0);

            break;
        }

        case EWsSdOpTwipsSize: {
            // This doesn't take any arguments
            eka2l1::vec2 screen_size = scr->size() * twips_mul;
            ctx.write_arg_pkg<eka2l1::vec2>(reply_slot, screen_size);
            ctx.set_request_status(0);

            break;
        }

        case EWsSdOpGetNumScreenModes: {
            ctx.set_request_status(scr->total_screen_mode());
            break;
        }

        case EWsSdOpGetScreenNumber: {
            ctx.set_request_status(scr->number);
            break;
        }

        case EWsSdOpSetScreenMode: {
            set_screen_mode(ctx, cmd);
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsSdOpSetScreenSizeAndRotation: {
            set_screen_mode_and_rotation(ctx, cmd);
            break;
        }

        case EWsSdOpGetScreenSizeModeList: {
            get_screen_size_mode_list(ctx, cmd);
            break;
        }

        // This get the screen size in pixels + twips and orientation for the given mode
        case EWsSdOpGetScreenModeSizeAndRotation: {
            get_screen_size_mode_and_rotation(ctx, cmd, true);
            break;
        }

        // This get the screen size in pixels and orientation for the given mode
        case EWsSdOpGetScreenModeSizeAndRotation2: {
            get_screen_size_mode_and_rotation(ctx, cmd, false);
            break;
        }

        case EWsSdOpGetScreenModeDisplayMode: {
            int mode = *reinterpret_cast<int *>(cmd.data_ptr);

            ctx.write_arg_pkg(reply_slot, scr->disp_mode);
            ctx.set_request_status(KErrNone);

            break;
        }

        // Get the current screen mode. AknCapServer uses this, compare with the saved screen mode
        // to trigger the layout change event for registered app.
        case EWsSdOpGetScreenMode: {
            ctx.set_request_status(scr->crr_mode);
            break;
        }

        case EWsSdOpFree: {
            ctx.set_request_status(KErrNone);
            client->delete_object(id);
            break;
        }

        default: {
            LOG_WARN("Unimplemented IPC call for screen driver: 0x{:x}", cmd.header.op);
            break;
        }
        }
    }
}
