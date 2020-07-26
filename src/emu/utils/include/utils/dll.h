/*
* Copyright (c) 2019 EKA2L1 Team.
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

#include <common/types.h>

#include <utils/des.h>
#include <utils/handle.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace eka2l1::epoc {
    struct ldr_info {
        uint32_t uid1;
        uint32_t uid2;
        uint32_t uid3;
        eka2l1::epoc::owner_type owner_type;
        int handle;
        uint32_t secure_id;
        uint32_t requested_version;
        int min_stack_size;
    };

    struct lib_info {
        uint16_t major;
        uint16_t minor;
        uint32_t uid1;
        uint32_t uid2;
        uint32_t uid3;
        uint32_t secure_id;
        uint32_t vendor_id;
        uint32_t caps[2];
    };

    struct lib_info2 : public lib_info {
        uint8_t hfp;
        uint8_t debug_attrib;
        uint8_t spare[6];
    };

    // I would not allocate 1684 bytes on stack for this struct...
    struct ldr_info_eka1 {
        std::uint32_t uid1_;
        std::uint32_t uid2_;
        std::uint32_t uid3_;
        std::uint32_t unk0C;
        std::uint32_t unk10[16];
        epoc::filename full_path_;
        epoc::filename filename_;
        epoc::buf_static<char16_t, 262> unk_;
        std::uint32_t result_handle;
        epoc::owner_type handle_owner_;
    };

    static_assert(sizeof(ldr_info_eka1) == 1660);
}
