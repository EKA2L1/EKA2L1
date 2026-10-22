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

#include <ldd/videodriver/videodriver.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/svc.h>

#include <common/log.h>
#include <utils/err.h>
#include <services/window/window.h>
#include <services/window/screen.h>
#include <kernel/chunk.h>
#include <system/hal.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace eka2l1::ldd {
    static const std::string VIDEO_DRIVER_FACTORY_NAME = "VideoDriver";

    video_driver_factory::video_driver_factory(kernel_system *kern, system *sys)
        : factory(kern, sys) {
    }

    void video_driver_factory::install() {
        obj_name = VIDEO_DRIVER_FACTORY_NAME;
    }

    std::unique_ptr<channel> video_driver_factory::make_channel(epoc::version ver) {
        return std::make_unique<video_driver_channel>(kern, sys_, ver);
    }

    video_driver_channel::video_driver_channel(kernel_system *kern, system *sys, epoc::version ver)
        : channel(kern, sys, ver) {
    }

    std::int32_t video_driver_channel::get_screen_number_of_colors(kernel::thread *r, const std::uint32_t n, const eka2l1::ptr<void> arg1,
        const eka2l1::ptr<void> arg2) {
        void *num_ptr = arg1.get(r->owning_process());
        if (!num_ptr) {
            return epoc::error_argument;
        }

        return epoc::do_hal_by_data_num(sys_, kernel::hal_data_eka1_screen_num_of_colors, num_ptr);
    }

    std::int32_t video_driver_channel::get_screen_info(kernel::thread *r, const std::uint32_t n, const eka2l1::ptr<void> arg1,
        const eka2l1::ptr<void> arg2) {
        void *data = arg1.get(r->owning_process());
        if (!data) {
            return epoc::error_argument;
        }

        return epoc::do_hal_by_data_num(sys_, kernel::hal_data_eka1_video_info, data);
    }

    std::int32_t video_driver_channel::do_control_epoc70(kernel::thread *r, const std::uint32_t n, const eka2l1::ptr<void> arg1,
        const eka2l1::ptr<void> arg2) {
        kernel::process *own = r->owning_process();
        int *value = reinterpret_cast<int *>(arg1.get(own));

        switch (n) {
        case video_driver_control_op_epoc70_get_backlight_state:
            if (!value) {
                return epoc::error_argument;
            }

            return epoc::do_hal(sys_, epoc::hal_category_display, epoc::display_hal_backlight_on, value, nullptr);

        case video_driver_control_op_epoc70_get_display_mode:
            if (!value) {
                return epoc::error_argument;
            }

            return epoc::do_hal(sys_, epoc::hal_category_display, epoc::display_hal_mode, value, nullptr);

        case video_driver_control_op_epoc70_get_mode_count:
            if (!value) {
                return epoc::error_argument;
            }

            // The screen is fixed to the mode the window server was configured with.
            *value = 1;
            return epoc::error_none;

        case video_driver_control_op_epoc70_get_screen_display_state:
            if (!value) {
                return epoc::error_argument;
            }

            *value = 1;
            return epoc::error_none;

        case video_driver_control_op_epoc70_get_screen_num_of_colors:
            return get_screen_number_of_colors(r, n, arg1, arg2);

        case video_driver_control_op_epoc70_get_current_mode_info:
            return get_screen_info(r, n, arg1, arg2);

        case video_driver_control_op_epoc70_get_specified_mode_info: {
            // The mode is passed by value here, the package to fill comes second.
            int mode = static_cast<int>(arg1.ptr_address());
            int *package = reinterpret_cast<int *>(arg2.get(own));

            if (!package) {
                return epoc::error_argument;
            }

            return epoc::do_hal(sys_, epoc::hal_category_display, epoc::display_hal_specified_mode_info,
                &mode, package);
        }

        default:
            break;
        }

        LOG_TRACE(LDD_MMCIF, "Unimplemented video driver control opcode {} (Symbian 7.0 table)", n);
        return epoc::error_not_supported;
    }

    std::int32_t video_driver_channel::do_control(kernel::thread *r, const std::uint32_t n, const eka2l1::ptr<void> arg1,
        const eka2l1::ptr<void> arg2) {
        if (kern->get_epoc_version() == epocver::epoc70) {
            return do_control_epoc70(r, n, arg1, arg2);
        }

        switch (n) {
        case video_driver_control_op_get_screen_num_of_colors:
            return get_screen_number_of_colors(r, n, arg1, arg2);

        case video_driver_control_op_get_screen_basic_info:
            return get_screen_info(r, n, arg1, arg2);
        
        default:
            break;
        }
        LOG_TRACE(LDD_MMCIF, "Unimplemented video driver control opcode {}", n);

        // Reporting success without filling the caller's value leaves it reading uninitialised memory.
        return epoc::error_not_supported;
    }

    std::int32_t video_driver_channel::do_request(epoc::notify_info info, const std::uint32_t n,
        const eka2l1::ptr<void> arg1, const eka2l1::ptr<void> arg2,
        const bool is_supervisor) {
        LOG_TRACE(LDD_MMCIF, "Unimplemented video driver request opcode {}", n);
        return 0;
    }

    static constexpr std::uint32_t LCD_UPDATE_CAPACITY = 0x1A0;

    struct lcd_update_queue {
        std::int32_t rectangles[LCD_UPDATE_CAPACITY][4];
        std::uint32_t count;
    };

    static_assert(sizeof(lcd_update_queue) == 0x1A04);

    struct lcd_context_info {
        eka2l1::ptr<lcd_update_queue> queue;
        std::uint32_t display_mode;
        std::uint32_t format;
        std::uint32_t privileged;
        std::uint32_t strides[4];
    };
    static_assert(sizeof(lcd_context_info) == 32);

    lcd_driver_factory::lcd_driver_factory(kernel_system *kern, system *sys)
        : factory(kern, sys) {
    }

    void lcd_driver_factory::install() {
        obj_name = "lcd";
    }

    std::unique_ptr<channel> lcd_driver_factory::make_channel(epoc::version ver) {
        return std::make_unique<lcd_driver_channel>(kern, sys_, ver);
    }

    lcd_driver_channel::lcd_driver_channel(kernel_system *kern, system *sys, epoc::version ver)
        : channel(kern, sys, ver) {
    }

    int lcd_driver_channel::destroy() {
        if (command_queue_ && !kern->wipeout_in_progress()) {
            command_queue_->decrease_access_count();
        }
        command_queue_ = nullptr;
        return channel::destroy();
    }

    std::int32_t lcd_driver_channel::do_control(kernel::thread *r, const std::uint32_t n,
        const eka2l1::ptr<void> arg1, const eka2l1::ptr<void> arg2) {
        auto *winserv = kern->get_by_name<window_server>(get_winserv_name_by_epocver(kern->get_epoc_version()));
        auto *screen = winserv ? winserv->get_current_focus_screen() : nullptr;
        auto *process = r->owning_process();
        if (!screen) {
            return epoc::error_not_ready;
        }
        switch (n) {
        case lcd_driver_create_context: {
            if (command_queue_) {
                return epoc::error_already_exists;
            }
            auto *context = arg1.cast<lcd_context_info>().get(process);
            auto *requested_mode = arg2.cast<std::uint32_t>().get(process);
            if (!context || !requested_mode) {
                return epoc::error_argument;
            }
            if (*requested_mode != static_cast<std::uint32_t>(screen->disp_mode)) {
                return epoc::error_not_supported;
            }
            auto *queue = kern->create<kernel::chunk>(kern->get_memory_system(), nullptr, "LCDUpdateQueue", 0,
                sizeof(lcd_update_queue), sizeof(lcd_update_queue), prot_read_write, kernel::chunk_type::normal,
                kernel::chunk_access::kernel_mapping, kernel::chunk_attrib::none);
            if (!queue) {
                return epoc::error_no_memory;
            }
            std::memset(queue->host_base(), 0, sizeof(lcd_update_queue));
            queue->increase_access_count();
            command_queue_ = queue;
            *context = {};
            // The ROM receives a user address, not a chunk handle.
            context->queue = queue->base(process).cast<lcd_update_queue>();
            context->display_mode = static_cast<std::uint32_t>(screen->disp_mode);
            const auto &mode = screen->current_mode();
            auto size = mode.size;
            if (mode.rotation % 180 != 0) {
                std::swap(size.x, size.y);
            }
            const auto bpp = epoc::get_bpp_from_display_mode(screen->disp_mode);
            for (int i = 0; i < 4; i++) {
                context->strides[i] = epoc::get_byte_width((i & 1) ? size.y : size.x, bpp);
            }
            return epoc::error_none;
        }
        case lcd_driver_submit_queue: {
            if (!command_queue_) {
                return epoc::error_not_supported;
            }
            auto *queue = static_cast<lcd_update_queue *>(command_queue_->host_base());
            if (queue->count > LCD_UPDATE_CAPACITY) {
                return epoc::error_corrupt;
            }
            std::vector<eka2l1::rect> rectangles;
            rectangles.reserve(queue->count);
            for (std::uint32_t i = 0; i < queue->count; i++) {
                const auto *entry = queue->rectangles[i];
                eka2l1::rect area(eka2l1::vec2(entry[0], entry[2]), eka2l1::vec2(entry[1], entry[3]));
                area.transform_to_symbian_rectangle();
                rectangles.push_back(area);
            }
            queue->count = 0;
            if (!rectangles.empty()) {
                std::uint64_t next_vsync = 0;
                screen->vsync(kern->get_ntimer(), next_vsync);
                screen->update_direct_framebuffer();
                screen->present_framebuffer(winserv->get_graphics_driver(), kern, rectangles.data(),
                    static_cast<std::uint32_t>(rectangles.size()));
            }
            return epoc::error_none;
        }
        case lcd_driver_get_framebuffer:
        case lcd_driver_get_orientation:
        case lcd_driver_get_scale: {
            auto *value = arg1.cast<std::uint32_t>().get(process);
            if (!value) {
                return epoc::error_argument;
            }
            if (n == lcd_driver_get_framebuffer) {
                *value = screen->screen_buffer_chunk->base(r->owning_process()).ptr_address()
                    + sizeof(std::uint16_t) * epoc::WORD_PALETTE_ENTRIES_COUNT;
                winserv->map_direct_framebuffer(screen);
            } else if (n == lcd_driver_get_orientation) {
                *value = screen->current_mode().rotation / 90;
            } else {
                *value = 1;
            }
            return epoc::error_none;
        }
        default:
            LOG_TRACE(LDD_MMCIF, "Unimplemented LCD driver control opcode {}", n);
            return epoc::error_not_supported;
        }
    }

    std::int32_t lcd_driver_channel::do_request(epoc::notify_info info, const std::uint32_t n,
        const eka2l1::ptr<void> arg1, const eka2l1::ptr<void> arg2, const bool is_supervisor) {
        info.complete(epoc::error_not_supported);
        return epoc::error_none;
    }
}
