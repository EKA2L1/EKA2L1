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

#include <epoc/services/applist/applist.h>
#include <common/buffer.h>

namespace eka2l1 {
    static bool read_str16_aligned(common::ro_stream *stream, std::u16string &dat) {
        char len = 0;

        // Read string length
        if (stream->read(&len, sizeof(len)) != sizeof(len)) {
            return false;
        }

        // Align if neccessary. Required align by 2.
        if (stream->tell() % 2 != 0) {
            stream->seek(1, common::seek_where::cur);
        }

        // Read the string
        dat.resize(len);
        if (stream->read(&dat[0], 2 * dat.length()) != 2 * dat.length()) {
            return false;
        }

        return true;
    }
    
    static bool read_mandatory_info(common::ro_stream *stream, apa_app_registry &reg) {
        // Skip over reserved variables
        stream->seek(8, common::seek_where::beg);
        std::u16string app_file;

        if (!read_str16_aligned(stream, app_file)) {
            return false;
        }

        return true;
    }

    bool read_registeration_info(common::ro_stream *stream, apa_app_registry &reg) {
        if (!read_mandatory_info(stream, reg)) {
            return false;
        }

        return true;
    }
}
