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

#include <ldd/hal/hal.h>

#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/svc.h>

#include <common/log.h>

namespace eka2l1::ldd {
    hal_factory::hal_factory(kernel_system *kern, system *sys)
        : factory(kern, sys) {
    }

    void hal_factory::install() {
        obj_name = HAL_FACTORY_NAME;
    }

    std::unique_ptr<channel> hal_factory::make_channel(epoc::version ver) {
        return std::make_unique<hal_channel>(kern, sys_, ver);
    }

    hal_channel::hal_channel(kernel_system *kern, system *sys, epoc::version ver)
        : channel(kern, sys, ver) {
    }

    std::int32_t hal_channel::do_control(kernel::thread *r, const std::uint32_t n, const eka2l1::ptr<void> arg1,
        const eka2l1::ptr<void> arg2) {
        switch (n) {
        case hal_control_opcode_get_value: {
            void *data = arg2.get(r->owning_process());
            if (!data) {
                LOG_ERROR(LDD_MMCIF, "HAL get destination data pointer is invalid!");
                return epoc::error_argument;
            }

            return epoc::do_hal_by_data_num(sys_, arg1.ptr_address(), data);
        }

        case hal_control_opcode_set_value:
            LOG_ERROR(LDD_MMCIF, "HAL set data is currently unsupported on the emulator!");
            break;

        default:
            LOG_TRACE(LDD_MMCIF, "Unimplemented control opcode {}", n);
            break;
        }

        return 0;
    }

    std::int32_t hal_channel::do_request(epoc::notify_info info, const std::uint32_t n,
        const eka2l1::ptr<void> arg1, const eka2l1::ptr<void> arg2,
        const bool is_supervisor) {
        LOG_TRACE(LDD_MMCIF, "Unimplemented request opcode {}", n);
        return 0;
    }
}