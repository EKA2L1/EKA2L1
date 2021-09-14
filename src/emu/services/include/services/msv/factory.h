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

#pragma once

#include <services/msv/operations/base.h>
#include <services/msv/common.h>

#include <cstdint>
#include <memory>

namespace eka2l1::epoc::msv {
    class operation_factory {
    private:
        msv_id mtm_uid_;

    public:
        explicit operation_factory(const msv_id id)
            : mtm_uid_(id) {
        }

        virtual std::shared_ptr<operation> create_operation(const msv_id operation_id, const operation_buffer &buffer,
            epoc::notify_info complete_info, const std::uint32_t command) = 0;

        const msv_id mtm_uid() const {
            return mtm_uid_;
        }
    };
}