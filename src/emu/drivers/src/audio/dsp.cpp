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

#include <drivers/audio/backend/ffmpeg/dsp_ffmpeg.h>
#include <drivers/audio/dsp.h>

namespace eka2l1::drivers {
    dsp_stream::dsp_stream()
        : samples_played_(0)
        , freq_(0)
        , channels_(0)
        , complete_callback_(nullptr)
        , buffer_copied_callback_(nullptr)
        , complete_userdata_(nullptr)
        , buffer_copied_userdata_(nullptr) {
    }

    std::unique_ptr<dsp_stream> new_dsp_out_stream(drivers::audio_driver *aud, const dsp_stream_backend dsp_backend) {
        switch (dsp_backend) {
        case dsp_stream_backend_ffmpeg:
            return std::make_unique<dsp_output_stream_ffmpeg>(aud);

        default:
            break;
        }

        return nullptr;
    }
}