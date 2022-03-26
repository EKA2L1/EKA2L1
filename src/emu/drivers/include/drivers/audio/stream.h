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

#include <cstdint>
#include <functional>

namespace eka2l1::drivers {
    using data_callback = std::function<std::size_t(std::int16_t *, std::size_t)>;
    class audio_driver;

    struct audio_output_stream {
    protected:
        audio_driver *driver_;
        std::size_t grand_volume_change_callback_handle_;

        std::uint32_t sample_rate;
        std::uint8_t channels;

    public:
        explicit audio_output_stream(audio_driver *driver, const std::uint32_t sample_rate, const std::uint8_t channels);
        virtual ~audio_output_stream();

        virtual bool start() = 0;
        virtual bool stop() = 0;
        virtual void pause() = 0;

        virtual bool is_playing() = 0;
        virtual bool is_pausing() = 0;

        virtual bool set_volume(const float volume) = 0;
        virtual float get_volume() const = 0;

        virtual bool current_frame_position(std::uint64_t *pos) = 0;

        const std::uint8_t get_channels() {
            return channels;
        }

        const std::uint32_t get_sample_rate() {
            return sample_rate;
        }
    };
};
