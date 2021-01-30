/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <ldd/ecomm/consts.h>
#include <ldd/ecomm/ecomm.h>

#include <kernel/thread.h>
#include <kernel/process.h>

#include <common/log.h>

namespace eka2l1::ldd {
    ecomm_factory::ecomm_factory(kernel_system *kern, system *sys)
        : factory(kern, sys) {
    }

    void ecomm_factory::install() {
        obj_name = ECOMM_FACTORY_NAME;
    }

    std::unique_ptr<channel> ecomm_factory::make_channel(epoc::version ver) {
        return std::make_unique<ecomm_channel>(kern, sys_, ver);
    }

    ecomm_channel::ecomm_channel(kernel_system *kern, system *sys, epoc::version ver)
        : channel(kern, sys, ver) {
    }

    std::int32_t ecomm_channel::do_control(kernel::thread *r, const std::uint32_t n, const eka2l1::ptr<void> arg1,
        const eka2l1::ptr<void> arg2) {
        LOG_TRACE(LDD_MMCIF, "Unimplemented control opcode {}", n);
        return 0;
    }

    std::int32_t ecomm_channel::do_request(epoc::notify_info info, const std::uint32_t n,
        const eka2l1::ptr<void> arg1, const eka2l1::ptr<void> arg2,
        const bool is_supervisor) {
        LOG_TRACE(LDD_MMCIF, "Unimplemented request opcode {}", n);
        return 0;
    }
}