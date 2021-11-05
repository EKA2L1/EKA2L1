/*
 * Copyright (c) 2021 EKA2L1 Team
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

#include <services/window/util.h>
#include <services/window/common.h>
#include <drivers/itc.h>

#include <common/log.h>

namespace eka2l1 {
    std::optional<common::region> get_region_from_context(service::ipc_context &ctx, ws_cmd &cmd) {
        if (cmd.header.cmd_len != 4) { 
            LOG_ERROR(SERVICE_WINDOW, "Region object's header data size is not 4 bytes!");
            return std::nullopt;
        }

        const std::int32_t count = *reinterpret_cast<std::int32_t*>(cmd.data_ptr);
        if (count < 0) {
            LOG_ERROR(SERVICE_WINDOW, "Rect count for region is invalid (expected >= 0 value, got {})", count);
            return std::nullopt;
        }

        const std::size_t region_rect_delivered_buffer_size = ctx.get_argument_data_size(remote_slot);

        if (region_rect_delivered_buffer_size < count * sizeof(eka2l1::rect)) {
            LOG_ERROR(SERVICE_WINDOW, "Rect buffer size is insufficient (expected {} * 16 bytes, got {} bytes)", count,
                region_rect_delivered_buffer_size);

            return std::nullopt;
        }

        std::uint8_t *region_rect_buffer_ptr = ctx.get_descriptor_argument_ptr(remote_slot);
        if (!region_rect_buffer_ptr) {
            LOG_ERROR(SERVICE_WINDOW, "Region rect buffer pointer is invalid!");
            return std::nullopt;
        }

        common::region return_result;
        for (std::int32_t i = 0; i < count; i++) {
            eka2l1::rect rect = *reinterpret_cast<eka2l1::rect*>(region_rect_buffer_ptr + i * sizeof(eka2l1::rect));
            rect.transform_from_symbian_rectangle();

            return_result.rects_.push_back(std::move(rect));
        }

        return return_result;
    }

    void clip_region(drivers::graphics_command_list_builder &cmd_builder, const common::region &region, const bool stencil_one_for_valid) {
        if (region.empty()) {
            return;
        }

        if (region.rects_.size() == 1) {
            eka2l1::rect rect_clip = region.rects_[0];

            cmd_builder.set_clipping(true);
            cmd_builder.clip_rect(rect_clip);

            return;
        }

        cmd_builder.set_stencil(true);

        // Try to fill region rects with 1 in stencil buffer.
        // Intentionally let stencil test fail so nothing gets draw. Just need to fill it after all.
        cmd_builder.set_stencil_pass_condition(drivers::stencil_face::back_and_front, drivers::condition_func::never,
            1, 0xFF);
        cmd_builder.set_stencil_action(drivers::stencil_face::back_and_front, drivers::stencil_action::replace,
            drivers::stencil_action::keep, drivers::stencil_action::keep);
        cmd_builder.set_stencil_mask(drivers::stencil_face::back_and_front, 0xFF);

        for (std::size_t i = 0; i < region.rects_.size(); i++) {
            if (region.rects_[i].valid()) {
                cmd_builder.draw_rectangle(region.rects_[i]);
            }
        }

        // Now set stencil buffer to only pass if value of pixel correspond in stencil buffer is not equal to 1 (invalid region),
        // or equal to 1 (if valid region)
        // Also disable writing to stencil buffer
        cmd_builder.set_stencil_pass_condition(drivers::stencil_face::back_and_front, stencil_one_for_valid ? drivers::condition_func::equal : drivers::condition_func::not_equal, 1, 0xFF);
        cmd_builder.set_stencil_action(drivers::stencil_face::back_and_front, drivers::stencil_action::keep,
            drivers::stencil_action::keep, drivers::stencil_action::keep);
    }
}