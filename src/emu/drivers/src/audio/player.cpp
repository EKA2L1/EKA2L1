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

#include <drivers/audio/player.h>
#include <drivers/audio/backend/ffmpeg/player_ffmpeg.h>
#include <drivers/audio/backend/wmf/player_wmf.h>
#include <common/platform.h>

#include <cstring>

namespace eka2l1::drivers {
    bool player::notify_any_done(finish_callback callback, std::uint8_t *data, const std::size_t data_size) {
        callback_ = callback;

        userdata_.resize(data_size);
        std::memcpy(userdata_.data(), data, data_size);

        return true;
    }

    std::uint8_t *player::get_notify_userdata(std::size_t *size_of_data) {
        if (userdata_.empty()) {
            return nullptr;
        }

        if (size_of_data) {
            *size_of_data = userdata_.size();
        }

        return userdata_.data();
    }
    
    std::unique_ptr<player> new_audio_player(audio_driver *aud, const player_type type) {
        switch (type) {
#if EKA2L1_PLATFORM(WIN32)
            case player_type_wmf:
                return std::make_unique<player_wmf>(aud);
#endif
            case player_type_ffmpeg:
                return std::make_unique<player_ffmpeg>(aud);

            default:
                break;
        }

        return nullptr;
    }
}