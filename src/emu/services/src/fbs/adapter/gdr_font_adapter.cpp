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

#include <services/fbs/adapter/gdr_font_adapter.h>

namespace eka2l1::epoc::adapter {
    gdr_font_file_adapter::gdr_font_file_adapter(std::vector<std::uint8_t> &data) {
        // Instantiate a read-only buffer stream
        buf_stream_ = std::make_unique<common::ro_buf_stream>(&data[0], data.size());
        
        if (!loader::gdr::parse_store(reinterpret_cast<common::ro_stream*>(buf_stream_.get()), store_)) {
            // Do this so a sanity check can happens
            buf_stream_.release();
        }
    }

    bool gdr_font_file_adapter::get_face_attrib(const std::size_t idx, open_font_face_attrib &face_attrib) {
        // Look for the index of the typeface
        if (!is_valid() || (idx >= store_.typefaces_.size())) {
            return false;
        }

        loader::gdr::typeface &the_typeface = store_.typefaces_[idx];

        // GDR only gives us name. For now assign all
        face_attrib.name.assign(nullptr, the_typeface.header_.name_);
        face_attrib.fam_name.assign(nullptr, the_typeface.header_.name_);
        face_attrib.local_full_name.assign(nullptr, the_typeface.header_.name_);
        face_attrib.local_full_fam_name.assign(nullptr, the_typeface.header_.name_);

        if (the_typeface.header_.flags_ & 0b01) {
            face_attrib.style |= open_font_face_attrib::bold;
        }

        if (the_typeface.header_.flags_ & 0b10) {
            face_attrib.style |= open_font_face_attrib::italic;
        }

        if (the_typeface.header_.flags_ & 0b100) {
            face_attrib.style |= open_font_face_attrib::serif;
        }

        std::memcpy(face_attrib.coverage, the_typeface.whole_coverage_, sizeof(face_attrib.coverage));
        return true;
    }
}