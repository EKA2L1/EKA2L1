/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <common/log.h>
#include <dispatch/dispatcher.h>
#include <dispatch/screen.h>

#include <drivers/graphics/graphics.h>
#include <epoc/epoc.h>
#include <kernel/kernel.h>
#include <services/window/common.h>
#include <services/window/window.h>

#include <fstream>

namespace eka2l1::dispatch {
    static constexpr std::uint32_t FPS_LIMIT = 60;

    BRIDGE_FUNC_DISPATCHER(void, update_screen, const std::uint32_t screen_number, const std::uint32_t num_rects, const eka2l1::rect *rect_list) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::graphics_driver *driver = sys->get_graphics_driver();
        kernel_system *kern = sys->get_kernel_system();

        epoc::screen *scr = dispatcher->winserv_->get_screens();

        while (scr != nullptr) {
            if (scr->number == screen_number) {
                // Update the DSA screen texture
                const eka2l1::vec2 screen_size = scr->size();
                const std::size_t buffer_size = scr->size().x * scr->size().y * 4;

                const std::lock_guard<std::mutex> guard(scr->screen_mutex);

                if (!scr->dsa_texture) {
                    scr->dsa_texture = drivers::create_bitmap(driver, screen_size, epoc::get_bpp_from_display_mode(scr->disp_mode));
                }

                auto command_list = driver->new_command_list();
                auto command_builder = driver->new_command_builder(command_list.get());

                command_builder->update_bitmap(scr->dsa_texture, reinterpret_cast<const char *>(scr->screen_buffer_chunk->host_base()), buffer_size,
                    { 0, 0 }, screen_size);

                command_builder->set_swizzle(scr->dsa_texture, drivers::channel_swizzle::red, drivers::channel_swizzle::green,
                    drivers::channel_swizzle::blue, drivers::channel_swizzle::one);

                driver->submit_command_list(*command_list);

                std::uint64_t next_vsync_us = 0;
                scr->vsync(sys->get_ntimer(), next_vsync_us);

                if (next_vsync_us) {
                    kern->crr_thread()->sleep(static_cast<std::uint32_t>(next_vsync_us));
                }
            }

            scr = scr->next;
        }
    }

    BRIDGE_FUNC_DISPATCHER(void, fast_blit, fast_blit_info *info) {
        kernel::process *crr_process = sys->get_kernel_system()->crr_process();
        info->src_blit_rect.transform_from_symbian_rectangle();

        std::uint8_t *dest_blt = info->dest_base.get(crr_process);
        const std::uint8_t *src_blt = info->src_base.get(crr_process);

        if ((info->src_size.x == info->src_blit_rect.size.x) && info->dest_stride == info->src_stride
            && info->src_blit_rect.top.x == 0 && info->dest_point.x == 0) {
            std::memcpy(dest_blt + info->dest_point.y * info->dest_stride, src_blt + info->src_blit_rect.top.x * info->src_stride,
                info->src_stride * info->src_size.y);
            return;
        }

        // Copy line by line, gurantee same mode already
        const std::int32_t bytes_per_pixel = info->src_stride / info->src_size.x;
        const std::uint32_t bytes_to_copy_per_line = bytes_per_pixel * info->src_blit_rect.size.x;

        for (int y = 0; y < info->src_blit_rect.size.y; y++) {
            std::memcpy(dest_blt + (info->dest_point.y + y) * info->dest_stride + info->dest_point.x * bytes_per_pixel,
                src_blt + (info->src_blit_rect.top.y + y) * info->src_stride + info->src_blit_rect.top.x * bytes_per_pixel,
                bytes_to_copy_per_line);
        }
    }
}