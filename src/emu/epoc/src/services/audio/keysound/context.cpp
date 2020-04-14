/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <common/chunkyseri.h>
#include <epoc/services/audio/keysound/context.h>

#include <algorithm>

namespace eka2l1::epoc::keysound {
    void context::do_it(common::chunkyseri &seri, const std::uint32_t info_count) {
        for (std::uint32_t i = 0; i < info_count; i++) {
            sound_trigger_info info;

            seri.absorb(info.sid);
            seri.absorb(info.key);
            seri.absorb(info.type);

            infos_.push_back(info);
        }

        // Sort them by key. The main focus of this server is to play key, so we gotta find the
        // correspond sound info as first priority.
        std::sort(infos_.begin(), infos_.end(), [](const sound_trigger_info &lhs, const sound_trigger_info &rhs) {
            return lhs.key < rhs.key;
        });
    }

    context::context(common::chunkyseri &info_seri, const std::uint32_t uid,
        const std::int32_t resource_id, const std::uint32_t info_count)
        : uid_(uid)
        , rsc_id_(resource_id) {
        do_it(info_seri, info_count);
    }

}