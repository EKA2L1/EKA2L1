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
#include <drivers/driver.h>

#include <cstdint>

namespace eka2l1::drivers {
    class audio_driver : public driver {
    public:
        virtual ~audio_driver() {}

        void run() override {}
        void abort() override {}

        /**
         * \brief Create a signed 16-bit LE audio output stream.
         * 
         * \param sample_rate       The target sample rate of output stream.
         * \param channels          The number of channels of the stream.
         * \param callback          The callback that the stream will use to retrive data.
         * 
         * \returns Instance to the stream on success.
         * 
         * \see     native_sample_rate
         */
        virtual std::unique_ptr<audio_output_stream> new_output_stream(const std::uint32_t sample_rate,
            const std::uint8_t channels, data_callback callback)
            = 0;

        virtual std::uint32_t native_sample_rate() = 0;
    };

    enum class audio_driver_backend {
        cubeb
    };

    using audio_driver_instance = std::unique_ptr<audio_driver>;
    audio_driver_instance make_audio_driver(const audio_driver_backend backend);
}
