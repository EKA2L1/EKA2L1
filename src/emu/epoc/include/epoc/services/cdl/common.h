/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <cstdint>
#include <epoc/utils/uid.h>
#include <string>
#include <vector>

namespace eka2l1::common {
    class chunkyseri;
}

namespace eka2l1::epoc {
    constexpr epoc::uid cdl_uid = 0x101F8243;

    struct cdl_ref {
        std::u16string name_;
        epoc::uid uid_;
        std::uint32_t id_;
    };

    using cdl_ref_collection = std::vector<cdl_ref>;

    void do_refs_state(common::chunkyseri &seri, cdl_ref_collection &collection_);
}
