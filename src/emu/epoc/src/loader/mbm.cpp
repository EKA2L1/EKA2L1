/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <common/buffer.h>
#include <epoc/loader/mbm.h>

namespace eka2l1::loader {
    bool mbm_file::valid() {
        return (header.uids.uid1 == 0x10000037) && (header.uids.uid2 == 0x10000042);
    }

    bool mbm_file::do_read_headers() {
        if (stream.read(&header, sizeof(header)) != sizeof(header)) {
            return false;
        }

        stream.seek(header.trailer_off, common::seek_where::beg);

        // Go to trailer, let's grab all those single bitmap header offsets
        if (stream.read(&trailer.count, sizeof(trailer.count)) != sizeof(trailer.count)) {
            return false;
        }

        if (trailer.count > 99) {
            return false;
        }

        trailer.sbm_offsets.resize(trailer.count);
        sbm_headers.resize(trailer.count);

        for (std::size_t i = 0; i < trailer.sbm_offsets.size(); i++) {
            if (stream.read(&trailer.sbm_offsets[i], 4) != 4) {
                return false;
            }

            // Remember the current offseet first
            const auto crr_offset = stream.tell();
            stream.seek(trailer.sbm_offsets[i], common::seek_where::beg);

            if (stream.read(&sbm_headers[i], sizeof(sbm_header)) != sizeof(sbm_header)) {
                return false;
            }

            stream.seek(crr_offset, common::seek_where::beg);
        }

        return valid();
    }
}
