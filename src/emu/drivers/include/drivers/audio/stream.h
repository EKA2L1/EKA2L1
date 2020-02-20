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

#include <functional>

namespace eka2l1::drivers {
    using data_callback = std::function<std::size_t(std::int16_t*, std::size_t)>;

    struct audio_output_stream {
        virtual ~audio_output_stream() {}

        virtual bool start() = 0;
        virtual bool stop() = 0;

        virtual bool is_playing() = 0;

        virtual bool set_volume(const float volume) = 0;
    };
};