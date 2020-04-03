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

#include <common/platform.h>

#if EKA2L1_PLATFORM(WIN32)

#include <drivers/audio/player.h>
#include <drivers/audio/stream.h>

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <windows.h>

namespace eka2l1::drivers {
    class audio_driver;

    enum player_wmf_request_type {
        player_wmf_request_raw_pcm = 0,
        player_wmf_request_format = 1
    };

    struct player_wmf_metadata {
        std::string key_;
        std::string value_;
    };

    struct player_wmf_request {
        IMFSourceReader *reader_;
        player_wmf_request_type type_;
        std::string url_;

        std::vector<std::uint8_t> data_;

        std::uint32_t freq_;
        std::uint32_t encoding_;
        std::uint32_t channels_;

        std::size_t data_pointer_;
        std::uint32_t flags_;

        std::int32_t repeat_left_;
        std::uint64_t silence_micros_;

        bool use_push_new_data_;

        explicit player_wmf_request()
            : data_pointer_(0)
            , flags_(0)
            , repeat_left_(0)
            , silence_micros_(0)
            , use_push_new_data_(false) {
        }
    };

    struct player_wmf: public player {
        audio_driver *aud_;

        std::unique_ptr<audio_output_stream> output_stream_;
        std::queue<player_wmf_request> requests_;
        std::mutex request_queue_lock_;

        std::vector<player_wmf_metadata> metadatas_;

    public:
        explicit player_wmf(audio_driver *driver);

        void get_more_data(player_wmf_request &request);
        std::size_t data_supply_callback(std::int16_t *data, std::size_t size);
        
        bool play() override;
        bool stop() override;

        bool queue_url(const std::string &url) override;

        bool queue_data(const char *raw_data, const std::size_t data_size,
            const std::uint32_t encoding_type, const std::uint32_t frequency,
            const std::uint32_t channels) override;

        bool notify_any_done(finish_callback callback, std::uint8_t *data, const std::size_t data_size) override;
        void clear_notify_done() override;
        
        void set_repeat(const std::int32_t repeat_times, const std::uint64_t silence_intervals_micros) override;
    };
}

#endif