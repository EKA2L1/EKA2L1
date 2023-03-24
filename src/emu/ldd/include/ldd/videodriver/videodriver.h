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

#pragma once

#include <kernel/ldd.h>

namespace eka2l1::ldd {
    enum video_driver_control_op {
        video_driver_control_op_get_screen_display_constrast = 3,
        video_driver_control_op_get_screen_display_constrast_max = 43,
        video_driver_control_op_get_screen_display_state = 13,
        video_driver_control_op_get_screen_num_of_colors = 14,
        video_driver_control_op_get_screen_basic_info = 15,
        video_driver_control_op_get_screen_extra_info = 16      // Same as 15, just fill other fields
    };

    class video_driver_channel : public channel {
    private:
        std::int32_t get_screen_number_of_colors(kernel::thread *r, const std::uint32_t n, const eka2l1::ptr<void> arg1,
            const eka2l1::ptr<void> arg2);
        std::int32_t get_screen_info(kernel::thread *r, const std::uint32_t n, const eka2l1::ptr<void> arg1,
            const eka2l1::ptr<void> arg2);
    public:
        explicit video_driver_channel(kernel_system *kern, system *sys, epoc::version ver);
        ~video_driver_channel() override {}

        std::int32_t do_control(kernel::thread *r, const std::uint32_t n, const eka2l1::ptr<void> arg1,
            const eka2l1::ptr<void> arg2) override;

        std::int32_t do_request(epoc::notify_info info, const std::uint32_t n,
            const eka2l1::ptr<void> arg1, const eka2l1::ptr<void> arg2,
            const bool is_supervisor) override;
    };

    /**
     * @brief MMC interface channel factory.
     */
    class video_driver_factory : public factory {
    public:
        explicit video_driver_factory(kernel_system *kern, system *sys);

        ~video_driver_factory() override {
        }

        void install() override;
        std::unique_ptr<channel> make_channel(epoc::version ver) override;
    };
}