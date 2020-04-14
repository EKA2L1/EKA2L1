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

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1::common {
    class chunkyseri;
}

namespace eka2l1::epoc::keysound {
    using sound_id = std::uint16_t;

    struct sound_trigger_info {
        sound_id sid;
        std::uint16_t key;
        std::uint8_t type;
    };

    enum sound_type {
        sound_type_file = 0,
        sound_type_sequence = 1,
        sound_type_tone = 2
    };

    struct sound_info {
        std::uint32_t id_;
        std::uint16_t priority_;
        std::uint32_t preference_;

        sound_type type_;

        std::u16string filename_;
        std::vector<std::uint8_t> sequences_;

        std::uint16_t freq_;
        std::uint32_t duration_;

        std::uint8_t volume_;
    };

    /**
     * A key sound context contains list of description for which key and which type of action
     * will trigger a sound registered in the server to be played.
     */
    struct context {
        std::vector<sound_trigger_info> infos_;

        std::uint32_t uid_;
        std::int32_t rsc_id_;

        explicit context(common::chunkyseri &info_seri, const std::uint32_t uid,
            const std::int32_t resource_id, const std::uint32_t info_count);

        /**
         * \brief Absorb key sound context info.
         * 
         * \param seri          Serializator reference.
         * \param info_count    Total of info available in the stream.
         */
        void do_it(common::chunkyseri &seri, const std::uint32_t info_count);
    };
}