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
#include <cubeb/cubeb.h>

namespace eka2l1::drivers {
    struct cubeb_audio_driver: public audio_driver {
    private:
        cubeb *context_;
        bool init_;

    public:
        explicit cubeb_audio_driver();

        ~cubeb_audio_driver() override;
        std::unique_ptr<audio_output_stream> new_output_stream(const std::uint32_t sample_rate,
            data_callback callback) override;
        std::uint32_t native_sample_rate() override;
    };
}