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

    struct player_wmf_request : public player_request_base {
        IMFSourceReader *reader_;
        IMFSinkWriter *writer_;

        explicit player_wmf_request()
            : reader_(nullptr) {
        }

        ~player_wmf_request();
    };

    struct player_wmf : public player_shared {
        explicit player_wmf(audio_driver *driver);

        void reset_request(player_request_instance &request) override;
        void get_more_data(player_request_instance &request) override;

        bool queue_url(const std::string &url) override;
        bool queue_data(const char *raw_data, const std::size_t data_size,
            const std::uint32_t encoding_type, const std::uint32_t frequency,
            const std::uint32_t channels) override;

        void read_and_transcode(player_request_instance &request, const std::uint32_t out_stream_idx, const std::uint64_t time_stamp_source, const std::uint64_t duration_source);
        bool set_position_for_custom_format(player_request_instance &request, const std::uint64_t pos_in_us) override;
        
        bool crop() override;
        bool record() override {
            return true;
        }
    };
}

#endif