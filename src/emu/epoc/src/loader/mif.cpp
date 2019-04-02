/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <epoc/loader/mif.h>
#include <common/buffer.h>

namespace eka2l1::loader {
    mif_file::mif_file(common::ro_stream *stream)
        : stream_(stream) {
    }

    bool mif_file::do_parse() {
        // Read version
        if (stream_->read(4, &header_.version, 4) != 4) {
            return false;
        }

        stream_->seek(0, common::seek_where::beg);

        switch (header_.version) {
        case 2: {
            if (stream_->read(&header_, sizeof(mif_header_v2)) != sizeof(mif_header_v2)) {
                return false;
            }

            break;
        }

        case 3: {
            if (stream_->read(&header_, sizeof(mif_header_v3)) != sizeof(mif_header_v3)) {
                return false;
            }

            break;
        }

        default: {
            return false;
        }
        }
        
        // Now, read indexes
        idxs_.resize(header_.array_len);
        stream_->read(header_.offset, &idxs_[0], sizeof(mif_index) * header_.array_len);

        return true;
    }

    bool mif_file::read_mif_entry(const std::size_t idx, std::uint8_t *buf, int &dest_size) {
        if (idx >= idxs_.size()) {
            return false;
        }

        if (buf == nullptr) {
            dest_size = idxs_[idx].len;
            return true; 
        }

        dest_size = static_cast<int>(stream_->read(idxs_[idx].offset, buf, common::min(dest_size, idxs_[idx].len)));
        return true;
    }
}
