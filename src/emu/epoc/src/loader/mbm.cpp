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

#include <common/bitmap.h>
#include <common/buffer.h>
#include <common/log.h>
#include <common/runlen.h>
#include <common/virtualmem.h>

#include <epoc/loader/mbm.h>

namespace eka2l1::loader {
    bool mbm_file::valid() {
        return (header.uids.uid1 == 0x10000037) && (header.uids.uid2 == 0x10000042);
    }

    bool mbm_file::do_read_headers() {
        if (stream->read(&header, sizeof(header)) != sizeof(header)) {
            return false;
        }

        stream->seek(header.trailer_off, common::seek_where::beg);

        // Go to trailer, let's grab all those single bitmap header offsets
        if (stream->read(&trailer.count, sizeof(trailer.count)) != sizeof(trailer.count)) {
            return false;
        }

        if (trailer.count > 99) {
            return false;
        }

        trailer.sbm_offsets.resize(trailer.count);
        sbm_headers.resize(trailer.count);

        for (std::size_t i = 0; i < trailer.sbm_offsets.size(); i++) {
            if (stream->read(&trailer.sbm_offsets[i], 4) != 4) {
                return false;
            }

            // Remember the current offseet first
            const auto crr_offset = stream->tell();
            stream->seek(trailer.sbm_offsets[i], common::seek_where::beg);

            if (stream->read(&sbm_headers[i], sizeof(sbm_header)) != sizeof(sbm_header)) {
                return false;
            }

            stream->seek(crr_offset, common::seek_where::beg);
        }

        return valid();
    }

    bool mbm_file::read_single_bitmap(const std::size_t index, std::uint8_t *dest, 
        std::size_t &dest_max) {
        if (index >= trailer.count) {
            return false;
        }
        
        sbm_header &single_bm_header = sbm_headers[index];
        const std::size_t data_offset = trailer.sbm_offsets[index] + single_bm_header.header_len;

        const auto crr_pos = stream->tell();
        stream->seek(data_offset, common::beg);

        bool success = true;

        std::size_t compressed_size = common::min(stream->left(), static_cast<std::size_t>(single_bm_header.compressed_len));

        switch (single_bm_header.compression) {
        case 0: {
            dest_max = compressed_size;
            
            if (dest) {
                stream->read(dest, dest_max);
            }

            break;
        }

        case 1: case 2: case 3: {
            LOG_ERROR("Unsupport RLE compression type {}", single_bm_header.compression);
            break;
        }

        case 4: {
            decompress_rle_24bit_stream(stream, compressed_size, dest, dest_max);
            break;
        }
        }

        stream->seek(crr_pos, common::beg);
        
        return true;
    }

    bool mbm_file::save_bitmap_to_file(const std::size_t index, const char *name) {
        std::size_t uncompressed_size = 0;
        
        if (!read_single_bitmap(index, nullptr, uncompressed_size)) {
            return false;
        }

        // Calculate uncompressed size first
        std::size_t bitmap_file_size = sizeof(common::bmp_header) + sizeof(common::dib_header_v1)
            + uncompressed_size;
        
        std::uint8_t *buf = reinterpret_cast<std::uint8_t*>(
            common::map_file(name, prot::read_write, bitmap_file_size));

        if (!buf) {
            return false;
        }

        common::bmp_header header;
        header.file_size = static_cast<std::uint32_t>(bitmap_file_size);
        header.pixel_array_offset = static_cast<std::uint32_t>(bitmap_file_size - uncompressed_size);
        
        common::wo_buf_stream stream(buf);
        stream.write(&header, sizeof(header));

        sbm_header &single_bm_header = sbm_headers[index];
        
        common::dib_header_v1 dib_header;
        dib_header.bit_per_pixels = single_bm_header.bit_per_pixels;
        dib_header.color_plane_count = 1;
        dib_header.comp = 0;
        dib_header.important_color_count = 0;
        dib_header.palette_count = single_bm_header.palette_size;
        dib_header.size = single_bm_header.size_pixels;
        dib_header.print_res = single_bm_header.size_twips;
        dib_header.uncompressed_size = static_cast<std::uint32_t>(uncompressed_size);

        // BMP are stored upside-down, use this to force them displays normally
        dib_header.size.y = -dib_header.size.y;
        dib_header.print_res.y = -dib_header.print_res.y;

        stream.write(&dib_header, dib_header.header_size);

        if (!read_single_bitmap(index, stream.get_current(), uncompressed_size)) {
            return false;
        }

        common::unmap_file(buf);

        return true;
    }
}
