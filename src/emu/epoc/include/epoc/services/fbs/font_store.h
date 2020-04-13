/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <epoc/services/fbs/adapter/font_adapter.h>
#include <epoc/services/fbs/font.h>

#include <unordered_map>
#include <vector>

namespace eka2l1 {
    class io_system;
}

namespace eka2l1::epoc {
    struct open_font_info {
        std::size_t idx;
        float scale_factor_x;
        float scale_factor_y;

        epoc::open_font_metrics metrics;
        epoc::adapter::font_file_adapter_base *adapter;
        epoc::open_font_face_attrib face_attrib;
    };

    // A set of fonts
    class font_store {
        std::unordered_map<std::u16string, std::vector<open_font_info>> open_font_store;
        std::vector<epoc::adapter::font_file_adapter_instance> font_adapters;

        eka2l1::io_system *io;

    protected:
        void folder_change_callback(const std::u16string &path, int action);

    public:
        explicit font_store(eka2l1::io_system *io)
            : io(io) {
        }

        void add_fonts(std::vector<std::uint8_t> &buf);
        open_font_info *seek_the_open_font(epoc::font_spec &spec);
    };
}