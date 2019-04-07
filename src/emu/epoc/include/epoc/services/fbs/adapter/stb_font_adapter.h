/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
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

#include <map>
#include <memory>
#include <vector>

#include <stb_truetype.h>
#include <epoc/services/fbs/adapter/font_adapter.h>

namespace eka2l1::epoc::adapter {
    class stb_font_file_adapter : public font_file_adapter_base {
        std::vector<std::uint8_t> data_;
        std::map<int, stbtt_fontinfo> cache_info;

        stbtt_fontinfo info_;
        std::size_t count_;
    
    public:
        stbtt_fontinfo *get_or_create_info(const int idx, int *off);

        explicit stb_font_file_adapter(std::vector<std::uint8_t> &data_);

        bool get_face_attrib(const std::size_t idx, open_font_face_attrib &face_attrib) override;
        bool get_metrics(const std::size_t idx, open_font_metrics &metrics) override;

        std::size_t count() override;
    };
}
