/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <services/sms/mtm/factory.h>
#include <services/sms/mtm/send.h>

namespace eka2l1::epoc::sms {
    std::shared_ptr<epoc::msv::operation> sms_operation_factory::create_operation(const epoc::msv::msv_id operation_id, const epoc::msv::operation_buffer &buffer,
        epoc::notify_info complete_info, const std::uint32_t command) {
        switch (command) {
        case command_code_schedule_copy:
            return std::make_shared<schedule_copy_operation>(operation_id, buffer, complete_info);

        default:
            break;
        }

        return nullptr;
    }
}