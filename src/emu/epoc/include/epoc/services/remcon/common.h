/*
 * Copyright (c) 2020 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
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

namespace eka2l1::epoc::remcon {
    enum player_type {
        player_type_audio = 1 << 0,
        player_type_video = 1 << 1,
        player_type_broadcast_audio = 1 << 2,
        player_type_broadcast_video = 1 << 3
    };

    enum player_subtype {
        player_subtype_none = 0,
        player_subtype_audio_book = 1 << 0,
        player_subtype_podcast = 1 << 1
    };
    
    enum remcon_message {
        remcon_message_set_player_type = 4
    };

    enum client_type {
        client_type_undefined = 0,
        client_type_controller = 1,
        client_type_target = 2
    };

    struct player_type_information {
        player_type type_;
        player_subtype subtype_;

        explicit player_type_information():
            type_(player_type_audio),
            subtype_(player_subtype_none) {
        }
            
    };

    const char *client_type_to_string(const client_type type);
    const char *player_type_to_string(const player_type type);
    const char *player_subtype_to_string(const player_subtype sub_type);
}