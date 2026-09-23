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
#include <system/hal.h>

#include <string>

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
}