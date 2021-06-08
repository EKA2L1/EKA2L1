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
    enum mmcif_control_op {
        mmcif_control_op_select_card = 4,
        mmcif_control_op_card_info = 6
    };

    enum mmcif_card_type : std::uint32_t {
        mmcif_card_type_rom,
        mmcif_card_type_flash,
        mmcif_card_type_io,
        mmcif_card_type_other,
        mmcif_card_type_unk
    };

    struct mmcif_card_legacy {
        std::uint32_t cid_[4];
        mmcif_card_type ctype_;
    };

    class mmcif_channel: public channel {
    public:
        explicit mmcif_channel(kernel_system *kern, system *sys, epoc::version ver);
        ~mmcif_channel() override {}

        std::int32_t do_control(kernel::thread *r, const std::uint32_t n, const eka2l1::ptr<void> arg1,
            const eka2l1::ptr<void> arg2) override;

        std::int32_t do_request(epoc::notify_info info, const std::uint32_t n,
            const eka2l1::ptr<void> arg1, const eka2l1::ptr<void> arg2,
            const bool is_supervisor) override;
    };

    /**
     * @brief MMC interface channel factory.
     */
    class mmcif_factory: public factory {
    public:
        explicit mmcif_factory(kernel_system *kern, system *sys);

        ~mmcif_factory() override {
        }

        void install() override;
        std::unique_ptr<channel> make_channel(epoc::version ver) override;
    };
}