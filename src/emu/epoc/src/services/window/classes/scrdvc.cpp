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
    screen_device::screen_device(window_server_client_ptr client, int number, drivers::graphics_driver *driver)
        : window_client_obj(client)
        , driver(driver)
        , screen(number)
        , focus(nullptr) {
        scr_config = client->get_ws().get_screen_config(number);
        crr_mode = &scr_config.modes[0];
    }

    epoc::window_group *screen_device::find_window_group_to_focus() {
        for (auto &win : windows) {
            if (win->type == window_kind::group) {
                if (reinterpret_cast<epoc::window_group*>(win)->can_receive_focus()) {
                    return reinterpret_cast<epoc::window_group*>(win);
                }
            }
        }

        return nullptr;
    }

    void screen_device::update_focus(epoc::window_group *closing_group) {
        epoc::window_group *next_to_focus = find_window_group_to_focus();

        if (next_to_focus != focus) {
            if (focus && focus != closing_group) {
                focus->lost_focus();
            }

            if (next_to_focus) {
                next_to_focus->gain_focus();
                focus = std::move(next_to_focus);
            }
        }

        client->get_ws().get_focus() = focus;

        // TODO: This changes the focus, so the window group list got updated
        // An event of that should be sent
    }

    static epoc::config::screen_mode *find_screen_mode(epoc::config::screen &scr, int mode_num) {
        for (std::size_t i = 0; i < scr.modes.size(); i++) {
            if (scr.modes[i].mode_number == mode_num) {
                return &scr.modes[i];
            }
        }

        return nullptr;
    }

    void screen_device::execute_command(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd cmd) {
        TWsScreenDeviceOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsSdOpPixelSize: {
            // This doesn't take any arguments
            eka2l1::vec2 screen_size = crr_mode->size;
            ctx.write_arg_pkg<eka2l1::vec2>(reply_slot, screen_size);
            ctx.set_request_status(0);

            break;
        }

        case EWsSdOpTwipsSize: {
            // This doesn't take any arguments
            eka2l1::vec2 screen_size = crr_mode->size * twips_mul;
            ctx.write_arg_pkg<eka2l1::vec2>(reply_slot, screen_size);
            ctx.set_request_status(0);

            break;
        }

        case EWsSdOpGetNumScreenModes: {
            ctx.set_request_status(static_cast<TInt>(scr_config.modes.size()));
            break;
        }

        case EWsSdOpSetScreenMode: {
            LOG_TRACE("Set screen mode stubbed, why did you even use this");
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsSdOpSetScreenSizeAndRotation: {
            pixel_twips_and_rot *info = reinterpret_cast<decltype(info)>(cmd.data_ptr);
            bool found = true;

            for (int i = 0; i < scr_config.modes.size(); i++) {
                if (scr_config.modes[i].size == info->pixel_size && number_to_orientation(scr_config.modes[i].rotation) == info->orientation) {
                    crr_mode = &scr_config.modes[i];

                    ctx.set_request_status(KErrNone);
                    found = true;

                    break;
                }
            }

            if (!found) {
                LOG_ERROR("Unable to set size: mode not found!");
                ctx.set_request_status(KErrNotSupported);
            }

            break;
        }

        case EWsSdOpGetScreenSizeModeList: {
            std::vector<int> modes;

            for (int i = 0; i < scr_config.modes.size(); i++) {
                modes.push_back(scr_config.modes[i].mode_number);
            }

            ctx.write_arg_pkg(reply_slot, reinterpret_cast<std::uint8_t *>(&modes[0]),
                static_cast<std::uint32_t>(sizeof(int) * modes.size()));

            ctx.set_request_status(static_cast<TInt>(scr_config.modes.size()));

            break;
        }

        // This get the screen size in pixels + twips and orientation for the given mode
        case EWsSdOpGetScreenModeSizeAndRotation: {
            int mode = *reinterpret_cast<int *>(cmd.data_ptr);

            epoc::config::screen_mode *scr_mode = find_screen_mode(scr_config, mode);

            if (!scr_mode) {
                ctx.set_request_status(KErrArgument);
                break;
            }

            pixel_twips_and_rot data;
            data.pixel_size = scr_mode->size;
            data.twips_size = scr_mode->size * twips_mul;
            data.orientation = number_to_orientation(scr_mode->rotation);

            ctx.write_arg_pkg(reply_slot, data);
            ctx.set_request_status(0);

            break;
        }

        // This get the screen size in pixels and orientation for the given mode
        case EWsSdOpGetScreenModeSizeAndRotation2: {
            int mode = *reinterpret_cast<int *>(cmd.data_ptr);

            epoc::config::screen_mode *scr_mode = find_screen_mode(scr_config, mode);

            if (!scr_mode) {
                ctx.set_request_status(KErrArgument);
                break;
            }

            pixel_and_rot data;
            data.pixel_size = scr_mode->size;
            data.orientation = number_to_orientation(scr_mode->rotation);

            ctx.write_arg_pkg(reply_slot, data);
            ctx.set_request_status(0);

            break;
        }

        case EWsSdOpGetScreenModeDisplayMode: {
            int mode = *reinterpret_cast<int *>(cmd.data_ptr);

            ctx.write_arg_pkg(reply_slot, disp_mode);
            ctx.set_request_status(KErrNone);

            break;
        }

        // Get the current screen mode. AknCapServer uses this, compare with the saved screen mode
        // to trigger the layout change event for registered app.
        case EWsSdOpGetScreenMode: {
            ctx.set_request_status(crr_mode->mode_number);
            break;
        }

        case EWsSdOpFree: {
            ctx.set_request_status(KErrNone);

            // Detach the screen device
            for (epoc::window *&win : windows) {
                win->dvc.reset();
            }

            windows.clear();

            // TODO: Remove reference in the client

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
