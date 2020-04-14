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

#include <common/buffer.h>
#include <common/bytepair.h>
#include <memory>
#include <vector>

namespace eka2l1::common {
    class ro_stream;
}

namespace eka2l1::scripting {
    struct ibytepair_stream_wrapper {
        common::ibytepair_stream bytepair_stream;
        std::unique_ptr<common::ro_stream> raw_fstream;

    public:
        ibytepair_stream_wrapper(const std::string &path);

        std::vector<uint32_t> get_all_page_offsets();
        void read_page_table();

        std::vector<char> read_page(uint16_t index);
        std::vector<char> read_pages();
    };
}
