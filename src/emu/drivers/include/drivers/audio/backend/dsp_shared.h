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

#include <drivers/audio/audio.h>
#include <drivers/audio/dsp.h>

namespace eka2l1::drivers {
    struct dsp_output_stream_shared: public dsp_output_stream {
    protected:
        drivers::audio_driver *aud_;
        std::unique_ptr<drivers::audio_output_stream> stream_;

        std::uint8_t channels_;

    public:
        explicit dsp_output_stream_shared(drivers::audio_driver *aud);
        
        std::size_t data_callback(std::int16_t *buffer, const std::size_t frame_count);

        virtual void volume(const std::uint32_t new_volume);
        virtual bool set_properties(const std::uint32_t freq, const std::uint8_t channels);

        virtual bool start();
        virtual bool stop();
    };
}