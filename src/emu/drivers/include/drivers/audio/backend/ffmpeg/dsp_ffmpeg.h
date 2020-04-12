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

#include <drivers/audio/backend/dsp_shared.h>

extern "C" {
    #include <libavcodec/avcodec.h>
}

namespace eka2l1::drivers {
    struct dsp_output_stream_ffmpeg: public dsp_output_stream_shared {
    protected:
        AVCodecContext *codec_;

    public:
        bool format(const four_cc fmt) override;

        void get_supported_formats(std::vector<four_cc> &cc_list) override;
        void decode_data(dsp_buffer &original, std::vector<std::uint8_t> &dest) override;
    };
}