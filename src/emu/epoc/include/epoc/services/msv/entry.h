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

#include <common/types.h>
#include <epoc/utils/uid.h>

#include <cstdint>
#include <string>

namespace eka2l1 {
    class io_system;
}

namespace eka2l1::epoc::msv {
    struct entry {
        std::uint32_t id_;
        std::int32_t parent_id_;
        std::uint32_t service_id_;
        epoc::uid type_uid_;
        epoc::uid mtm_uid_;
        std::uint32_t data_;
        std::u16string description_;
        std::u16string details_;
        std::uint64_t time_;
    };

    struct entry_indexer {
        std::vector<entry> entries_;
        io_system *io_;
        drive_number rom_drv_;
        language preferred_lang_;

    protected:
        bool create_standard_entries(drive_number crr_drive);
        bool create_root_entry();

    public:
        explicit entry_indexer(io_system *io, const language preferred_lang);
        bool add_entry(entry &ent);
    };
}