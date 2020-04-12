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

#include <drivers/audio/stream.h>
#include <cubeb/cubeb.h>

namespace eka2l1::drivers {
    struct cubeb_audio_output_stream: public audio_output_stream {
    private:
        cubeb_stream *stream_;
        data_callback callback_;
        bool playing_;

    public:
        explicit cubeb_audio_output_stream(cubeb *context_, const std::uint32_t sample_rate,
            const std::uint8_t channels, data_callback callback);

        ~cubeb_audio_output_stream() override;

        std::size_t call_callback(std::int16_t *output_buffer, const long frames);

        bool start() override;
        bool stop() override;

        bool is_playing() override;
        
        bool set_volume(const float volume) override;
    };
}
