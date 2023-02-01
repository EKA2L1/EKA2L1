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

#include <ldd/ekeyb/ekeyb.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/svc.h>

#include <common/log.h>
#include <utils/err.h>
#include <services/window/window.h>

#include <string>

namespace eka2l1::ldd {
    static const std::string EKEYB_FACTORY_NAME = "EKeyB";

    ekeyb_factory::ekeyb_factory(kernel_system *kern, system *sys)
        : factory(kern, sys) {
    }

    void ekeyb_factory::install() {
        obj_name = EKEYB_FACTORY_NAME;
    }

    std::unique_ptr<channel> ekeyb_factory::make_channel(epoc::version ver) {
        return std::make_unique<ekeyb_channel>(kern, sys_, ver);
    }

    ekeyb_channel::ekeyb_channel(kernel_system *kern, system *sys, epoc::version ver)
        : channel(kern, sys, ver) {
    }

    std::int32_t ekeyb_channel::do_control(kernel::thread *r, const std::uint32_t n, const eka2l1::ptr<void> arg1,
        const eka2l1::ptr<void> arg2) {
        LOG_TRACE(LDD_MMCIF, "Unimplemented ekeyb control opcode {}", n);
        return 0;
    }

    std::int32_t ekeyb_channel::do_request(epoc::notify_info info, const std::uint32_t n,
        const eka2l1::ptr<void> arg1, const eka2l1::ptr<void> arg2,
        const bool is_supervisor) {
        LOG_TRACE(LDD_MMCIF, "Unimplemented ekeyb request opcode {}", n);
        return 0;
    }
}