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

#include <epoc/loader/mbm.h>
#include <epoc/loader/mif.h>

#include <pybind11/pybind11.h>

#include <memory>
#include <string>

namespace eka2l1::common {
    class ro_stream;
}

namespace eka2l1::scripting {
    class mbm_reader {
        std::unique_ptr<loader::mbm_file> mbm_;
        std::unique_ptr<common::ro_stream> stream_;
    
    public:
        explicit mbm_reader(const std::string &path);

        std::uint32_t bitmap_count();
        bool save_bitmap(const std::uint32_t idx, const std::string &dest_path);

        std::uint32_t bits_per_pixel(const std::uint32_t idx);
        std::pair<int, int> bitmap_size(const std::uint32_t idx);
    };

    class mif_reader {
        std::unique_ptr<loader::mif_file> mif_;
        std::unique_ptr<common::ro_stream> stream_;
    
    public:
        explicit mif_reader(const std::string &path);

        std::uint32_t entry_count();
        int entry_data_size(const std::size_t idx);
        pybind11::bytes read_entry(const std::size_t idx);
    };
}
