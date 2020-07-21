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

#include <services/etel/common.h>
#include <services/etel/line.h>

#include <vector>

namespace eka2l1 {
    struct etel_line;

    struct etel_phone : public etel_entity {
        epoc::etel_phone_status status_;
        epoc::etel_phone_info info_;
        epoc::etel_phone_network_info network_info_;

        std::vector<etel_line *> lines_;

    public:
        explicit etel_phone(const epoc::etel_phone_info &info);
        ~etel_phone() override;

        bool init();

        epoc::etel_entry_type type() const override {
            return epoc::etel_entry_phone;
        }
    };
}