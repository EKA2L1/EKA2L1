/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <common/log.h>
#include <imgui.h>
#include <vector>

#include <cstdint>

namespace eka2l1 {
    namespace imgui {
        class logger : public eka2l1::base_logger {
            std::vector<uint32_t> line_offsets;

            ImGuiTextBuffer text_buffer;
            ImGuiTextFilter text_filter;

            bool scroll_to_bottom;

        public:
            void clear() override;
            void log(const char *fmt, ...) override;

            void draw(const char *title, bool *p_opened = NULL);
        };
    }
}

