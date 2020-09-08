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

#include <drivers/audio/backend/player_shared.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace eka2l1::drivers {
    struct player_ffmpeg_request : public player_request_base {
        AVCodecContext *codec_;
        AVFormatContext *format_;
        AVPacket packet_;

        explicit player_ffmpeg_request()
            : codec_(nullptr)
            , format_(nullptr) {
        }

        ~player_ffmpeg_request();

        void deinit();
    };

    struct player_ffmpeg : public player_shared {
    public:
        explicit player_ffmpeg(audio_driver *driver);
        ~player_ffmpeg() override;

        void reset_request(player_request_instance &request) override;
        void get_more_data(player_request_instance &request) override;

        bool queue_url(const std::string &url) override;

        bool queue_data(const char *raw_data, const std::size_t data_size,
            const std::uint32_t encoding_type, const std::uint32_t frequency,
            const std::uint32_t channels) override;

        void set_position_for_custom_format(player_request_instance &request, const std::uint64_t pos_in_us) override;
    };
}