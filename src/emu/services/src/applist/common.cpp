/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <services/applist/common.h>
#include <common/buffer.h>

namespace eka2l1 {
    bool apa_capability::internalize(common::ro_stream &stream) {
        std::uint32_t ver = 0;
        if (stream.read(&ver, 4) != 4) {
            return false;
        }

        std::int32_t cap = 0;
        if (stream.read(&cap, 4) != 4) {
            return false;
        }

        ability = static_cast<embeddability>(cap);

        std::int32_t boolean_val = 0;
        if (stream.read(&boolean_val, 4) != 4) {
            return false;
        }

        support_being_asked_to_create_new_file = static_cast<bool>(boolean_val);

        if (stream.read(&boolean_val, 4) != 4) {
            return false;
        }

        is_hidden = static_cast<bool>(boolean_val);
        launch_in_background = false;

        if (ver == 1) {
            return true;
        }

        if (stream.read(&boolean_val, 4) != 4) {
            return false;
        }

        launch_in_background = static_cast<bool>(boolean_val);

        if (ver == 2) {
            return true;
        }

        std::u16string the_group_name_just_read;
        if (!epoc::read_des_string(the_group_name_just_read, &stream, true)) {
            return false;
        }

        group_name.assign(nullptr, the_group_name_just_read);

        if (ver == 3) {
            return true;
        }

        if (stream.read(&flags, 4) != 4) {
            return false;
        }

        return true;
    }
}