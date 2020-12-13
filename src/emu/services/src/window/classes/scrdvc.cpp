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

#include <services/window/classes/scrdvc.h>
#include <services/window/window.h>

#include <services/window/classes/winbase.h>
#include <services/window/classes/wingroup.h>

#include <services/window/op.h>
#include <services/window/opheader.h>
#include <utils/err.h>

namespace eka2l1::epoc {
    static epoc::graphics_orientation get_orientation_from_rotation(const int rotate) {
        switch (rotate) {
        case 0:
            return epoc::graphics_orientation::normal;

        case 90:
            return epoc::graphics_orientation::rotated90;

        case 180:
            return epoc::graphics_orientation::rotated180;

        case 270:
            return epoc::graphics_orientation::rotated270;

        default:
            break;
        }

        LOG_ERROR(SERVICE_WINDOW, "Cannot translate rotation {} to an orientation, returns default", rotate);
        return epoc::graphics_orientation::normal;
    }

    screen_device::screen_device(window_server_client_ptr client, epoc::screen *scr)
        : window_client_obj(client, scr) {
    }

    void screen_device::set_screen_mode_and_rotation(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        pixel_twips_and_rot *info = reinterpret_cast<decltype(info)>(cmd.data_ptr);

        for (int i = 0; i < scr->scr_config.modes.size(); i++) {
            epoc::config::screen_mode &mode = scr->scr_config.modes[i];

            if (mode.size == info->pixel_size && number_to_orientation(mode.rotation) == info->orientation) {
                // Eureka... Bắt được mày rồi....
                scr->set_screen_mode(client->get_ws().get_graphics_driver(), mode.mode_number);
                ctx.complete(epoc::error_none);
                return;
            }
        }

        LOG_ERROR(SERVICE_WINDOW, "Unable to set size and orientation: mode not found!");
        ctx.complete(epoc::error_not_supported);
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

        ctx.write_data_to_descriptor_argument(reply_slot, reinterpret_cast<std::uint8_t *>(&modes[0]),
            static_cast<std::uint32_t>(sizeof(int) * modes.size()));

        ctx.complete(scr->total_screen_mode());
    }

    void screen_device::get_screen_size_mode_and_rotation(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd,
        const bool bonus_the_twips) {
        const int mode = *reinterpret_cast<int *>(cmd.data_ptr);
        const epoc::config::screen_mode *scr_mode = scr->mode_info(mode);

        if (!scr_mode) {
            ctx.complete(epoc::error_argument);
            return;
        }

        if (bonus_the_twips) {
            pixel_twips_and_rot data;
            data.pixel_size = scr_mode->size;
            data.twips_size = scr_mode->size * twips_mul;
            data.orientation = number_to_orientation(scr_mode->rotation);

            ctx.write_data_to_descriptor_argument(reply_slot, data);
        } else {
            pixel_and_rot data;
            data.pixel_size = scr_mode->size;
            data.orientation = number_to_orientation(scr_mode->rotation);

            ctx.write_data_to_descriptor_argument(reply_slot, data);
        }

        ctx.complete(epoc::error_none);
    }

    void screen_device::get_default_screen_size_and_rotation(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd,
        const bool twips) {
        const epoc::config::screen_mode &mode = scr->current_mode();

        pixel_and_rot result;
        result.pixel_size = mode.size;
        result.orientation = get_orientation_from_rotation(mode.rotation);

        if (twips) {
            result.pixel_size = result.pixel_size * 15;
        }

        ctx.write_data_to_descriptor_argument<pixel_and_rot>(reply_slot, result);
        ctx.complete(0);
    }

    void screen_device::get_default_screen_mode_origin(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        // On emulator there is no physical scale nor coordinate
        ctx.write_data_to_descriptor_argument<eka2l1::vec2>(reply_slot, eka2l1::vec2(0, 0));
        ctx.complete(0);
    }

    void screen_device::get_current_screen_mode_scale(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        // On emulator there is no physical scale nor coordinate
        ctx.write_data_to_descriptor_argument<eka2l1::vec2>(reply_slot, eka2l1::vec2(1, 1));
        ctx.complete(0);
    }

    void screen_device::is_screen_mode_dynamic(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        const epoc::config::screen_mode &mode = scr->current_mode();

        // The request code set is the boolean for whether screen mode is dynamic
        if ((mode.size.x == -1) && (mode.size.y == -1)) {
            // Dynamiccc!!!!
            ctx.complete(1);
            return;
        }

        ctx.complete(0);
    }

    void screen_device::get_rotation_list(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        // Each bit corresponds to its ordinal in graphics_orientation enum
        std::int32_t mode_list = 0;

        for (int i = 0; i < scr->total_screen_mode(); i++) {
            const epoc::config::screen_mode *mode = scr->mode_info(i + 1);

            if (mode) {
                mode_list |= (1 << static_cast<std::int32_t>(get_orientation_from_rotation(mode->rotation)));
            }
        }

        ctx.complete(mode_list);
    }

    void screen_device::execute_command(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        ws_screen_device_opcode op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case ws_sd_op_pixel_size: {
            // This doesn't take any arguments
            ctx.write_data_to_descriptor_argument<eka2l1::vec2>(reply_slot, scr->current_mode().size);
            ctx.complete(0);

            break;
        }

        case ws_sd_op_twips_size: {
            // This doesn't take any arguments
            eka2l1::vec2 screen_size = scr->current_mode().size * twips_mul;
            ctx.write_data_to_descriptor_argument<eka2l1::vec2>(reply_slot, screen_size);
            ctx.complete(0);

            break;
        }

        case ws_sd_op_get_default_screen_size_and_rotation:
            get_default_screen_size_and_rotation(ctx, cmd, true);
            break;

        case ws_sd_op_get_default_screen_size_and_rotation2:
            get_default_screen_size_and_rotation(ctx, cmd, false);
            break;

        case ws_sd_op_get_screen_mode_scale:
        case ws_sd_op_get_current_screen_mode_scale:
            get_current_screen_mode_scale(ctx, cmd);
            break;

        case ws_sd_op_get_default_screen_mode_origin:
            get_default_screen_mode_origin(ctx, cmd);
            break;

        case ws_sd_op_get_num_screen_modes: {
            ctx.complete(scr->total_screen_mode());
            break;
        }

        case ws_sd_op_get_screen_number: {
            ctx.complete(scr->number);
            break;
        }

        case ws_sd_op_set_screen_mode: {
            set_screen_mode(ctx, cmd);
            ctx.complete(epoc::error_none);

            break;
        }

        case ws_sd_op_set_screen_size_and_rotation: {
            set_screen_mode_and_rotation(ctx, cmd);
            break;
        }

        case ws_sd_op_get_screen_size_mode_list: {
            get_screen_size_mode_list(ctx, cmd);
            break;
        }

        // This get the screen size in pixels + twips and orientation for the given mode
        case ws_sd_op_get_screen_mode_size_and_rotation: {
            get_screen_size_mode_and_rotation(ctx, cmd, true);
            break;
        }

        // This get the screen size in pixels and orientation for the given mode
        case ws_sd_op_get_screen_mode_size_and_rotation2: {
            get_screen_size_mode_and_rotation(ctx, cmd, false);
            break;
        }

        case ws_sd_op_get_screen_mode_display_mode: {
            int mode = *reinterpret_cast<int *>(cmd.data_ptr);

            ctx.write_data_to_descriptor_argument(reply_slot, scr->disp_mode);
            ctx.complete(epoc::error_none);

            break;
        }

        // Get the current screen mode. AknCapServer uses this, compare with the saved screen mode
        // to trigger the layout change event for registered app.
        case ws_sd_op_get_screen_mode: {
            ctx.complete(scr->crr_mode);
            break;
        }

        case ws_sd_op_display_mode: {
            ctx.complete(static_cast<int>(scr->disp_mode));
            break;
        }

        case ws_sd_op_free: {
            ctx.complete(epoc::error_none);
            client->delete_object(cmd.obj_handle);
            break;
        }

        case ws_sd_op_is_screen_mode_dynamic:
            is_screen_mode_dynamic(ctx, cmd);
            break;

        case ws_sd_op_get_rotation_list:
            get_rotation_list(ctx, cmd);
            break;

        default: {
            LOG_WARN(SERVICE_WINDOW, "Unimplemented IPC call for screen driver: 0x{:x}", cmd.header.op);
            break;
        }
        }
    }
}
