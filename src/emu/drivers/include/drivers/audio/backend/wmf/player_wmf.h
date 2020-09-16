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

#include <drivers/audio/backend/player_shared.h>

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
    class rw_stream_com;

    struct player_wmf_request : public player_request_base {
        IMFSourceReader *reader_;
        IMFSinkWriter *writer_;

        IMFCollection *output_supported_list_;
        IMFMediaType *output_type_;

        rw_stream_com *custom_stream_;

        explicit player_wmf_request()
            : reader_(nullptr)
            , writer_(nullptr)
            , output_supported_list_(nullptr)
            , output_type_(nullptr)
            , custom_stream_(nullptr) {
        }

        bool set_output_type(IMFMediaType *new_output_type);

        ~player_wmf_request();
    };

    struct player_wmf : public player_shared {
    protected:
        bool is_ready_to_play(player_request_instance &request) override;

    public:
        explicit player_wmf(audio_driver *driver);
        
        bool make_backend_source(player_request_instance &request) override;

        void reset_request(player_request_instance &request) override;
        void get_more_data(player_request_instance &request) override;

        bool queue_url(const std::string &url) override;
        bool queue_custom(common::rw_stream *stream) override;

        void read_and_transcode(player_request_instance &request, const std::uint32_t out_stream_idx, const std::uint64_t time_stamp_source, const std::uint64_t duration_source);
        bool set_position_for_custom_format(player_request_instance &request, const std::uint64_t pos_in_us) override;
        
        bool crop() override;
        bool record() override;
        
        bool set_dest_encoding(const std::uint32_t enc) override;
        bool set_dest_freq(const std::uint32_t freq) override;
        bool set_dest_channel_count(const std::uint32_t cn) override;
    };
}

#endif