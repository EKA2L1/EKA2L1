/*
 * Copyright (c) 2020 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <cstdint>
#include <epoc/ptr.h>
#include <epoc/utils/uid.h>

namespace eka2l1::kernel {
    class process;
}

namespace eka2l1::epoc {
    struct cdl_main {
        std::int32_t major_;
        std::int32_t minor_;
        eka2l1::ptr<void> data_;
    };

    struct cdl_array {
        std::int32_t count_;
        eka2l1::ptr<void> data_;
    };

    struct cdl_interface {
        std::int32_t major_eng_ver_;
        std::int32_t minor_eng_ver_;
        eka2l1::ptr<void> name_;
        epoc::uid uid_;
        std::int32_t major_ver_;
        std::int32_t minor_ver_;
        std::uint32_t flags_;
        std::int32_t api_size_;
        eka2l1::ptr<void> spare_;
    };

    struct cdl_customisation {
        std::int32_t id_;
        eka2l1::ptr<cdl_interface> interface_;
        eka2l1::ptr<void> impl_;
    };

    cdl_customisation *get_customisation(kernel::process *pr, cdl_main *main, const std::int32_t cust_index);
    void *get_data_from_customisation(kernel::process *pr, cdl_customisation *cust, const std::int32_t data_index);
}