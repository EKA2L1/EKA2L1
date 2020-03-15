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

#include <epoc/services/remcon/common.h>

namespace eka2l1::epoc::remcon {
    const char *player_type_to_string(const player_type type) {
        switch (type) {
        case player_type_audio:
            return "Audio";

        case player_type_video:
            return "Video";

        case player_type_broadcast_audio:
            return "Broadcasting audio";

        case player_type_broadcast_video:
            return "Broadcasting video";

        default:
            break;
        }

        return "Unknown";
    }

    const char *player_subtype_to_string(const player_subtype sub_type) {
        switch (sub_type) {
        case player_subtype_none:
            return "None";

        case player_subtype_audio_book:
            return "Audio book";

        case player_subtype_podcast:
            return "Podcast";

        default:
            break;
        }

        return "Unknown";
    }
    
    const char *client_type_to_string(const client_type type) {
        switch (type) {
        case client_type_undefined:
            return "Undefined";

        case client_type_controller:
            return "Controller";

        case client_type_target:
            return "Target";

        default:
            break;
        }

        return "Unknown";
    }
}