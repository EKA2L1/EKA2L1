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

#include <common/algorithm.h>
#include <common/buffer.h>
#include <loader/mif.h>
#include <loader/nvg.h>
#include <loader/svgb.h>

#include <cstring>
#include <miniz.h>

namespace eka2l1::loader {
    // Upper bound on what a single icon may inflate to. The gzip trailer's size field
    // is attacker-controlled (and only holds the length modulo 2^32), so it can't be
    // trusted to size the destination on its own.
    static constexpr std::size_t MAX_INFLATED_ICON_SIZE = 64 * 1024 * 1024;

    static constexpr std::size_t GZIP_HEADER_SIZE = 10;
    static constexpr std::size_t GZIP_TRAILER_SIZE = 8;

    enum gzip_flag {
        gzip_flag_hcrc = 0x02,
        gzip_flag_extra = 0x04,
        gzip_flag_name = 0x08,
        gzip_flag_comment = 0x10
    };

    // Inflate a gzip stream. miniz's inflate only understands zlib-wrapped or raw
    // deflate, so the gzip header is skipped by hand and the body run as raw deflate.
    static bool decompress_gzip(const std::uint8_t *data, const std::size_t size,
        std::vector<std::uint8_t> &dest) {
        if ((size < GZIP_HEADER_SIZE + GZIP_TRAILER_SIZE) || (data[0] != 0x1F) || (data[1] != 0x8B)
            || (data[2] != 0x08)) {
            return false;
        }

        const std::uint8_t flags = data[3];
        std::size_t pos = GZIP_HEADER_SIZE;

        if (flags & gzip_flag_extra) {
            if (pos + 2 > size) {
                return false;
            }

            pos += 2 + (static_cast<std::size_t>(data[pos]) | (static_cast<std::size_t>(data[pos + 1]) << 8));
        }

        for (const std::uint8_t str_flag : { gzip_flag_name, gzip_flag_comment }) {
            if (flags & str_flag) {
                while ((pos < size) && data[pos]) {
                    pos++;
                }

                pos++;
            }
        }

        if (flags & gzip_flag_hcrc) {
            pos += 2;
        }

        if (pos + GZIP_TRAILER_SIZE >= size) {
            return false;
        }

        // The trailer's ISIZE is a good first guess for the output size; grow later if
        // the real content turns out to be larger.
        const std::uint8_t *trailer = data + size - GZIP_TRAILER_SIZE;
        std::size_t reserved = static_cast<std::size_t>(trailer[4]) | (static_cast<std::size_t>(trailer[5]) << 8)
            | (static_cast<std::size_t>(trailer[6]) << 16) | (static_cast<std::size_t>(trailer[7]) << 24);

        if (reserved == 0) {
            reserved = (size - pos) * 4;
        }

        reserved = common::min<std::size_t>(common::max<std::size_t>(reserved, 1024), MAX_INFLATED_ICON_SIZE);

        mz_stream stream;
        std::memset(&stream, 0, sizeof(stream));

        if (mz_inflateInit2(&stream, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK) {
            return false;
        }

        stream.next_in = data + pos;
        stream.avail_in = static_cast<unsigned int>(size - pos - GZIP_TRAILER_SIZE);

        dest.resize(reserved);
        int res = MZ_OK;

        while (true) {
            stream.next_out = dest.data() + stream.total_out;
            stream.avail_out = static_cast<unsigned int>(dest.size() - stream.total_out);

            res = mz_inflate(&stream, MZ_SYNC_FLUSH);

            if (((res == MZ_OK) || (res == MZ_BUF_ERROR)) && (stream.total_out == dest.size())) {
                if (dest.size() >= MAX_INFLATED_ICON_SIZE) {
                    res = MZ_BUF_ERROR;
                    break;
                }

                dest.resize(common::min<std::size_t>(dest.size() * 2, MAX_INFLATED_ICON_SIZE));
                continue;
            }

            break;
        }

        mz_inflateEnd(&stream);

        if (res != MZ_STREAM_END) {
            dest.clear();
            return false;
        }

        dest.resize(static_cast<std::size_t>(stream.total_out));
        return true;
    }

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

    bool convert_mif_icon_to_svg(std::uint8_t *entry, const std::size_t entry_size,
        common::wo_stream &out, nvg_options *nvg_opts) {
        if (!entry || (entry_size <= sizeof(mif_icon_header))) {
            return false;
        }

        mif_icon_header header;
        std::memcpy(&header, entry, sizeof(header));

        std::uint8_t *payload = entry + sizeof(header);
        const std::size_t payload_size = entry_size - sizeof(header);

        if (header.type == mif_icon_type_nvg) {
            common::ro_buf_stream in(payload, payload_size);
            std::vector<nvg_convert_error_description> errors;

            return convert_nvg_to_svg(in, out, errors, nvg_opts);
        }

        if (header.type != mif_icon_type_svg) {
            // Raster (mif_icon_type_bmp) entries have no SVG form.
            return false;
        }

        // Some icons ship as a gzip-wrapped plain SVG rather than the binarised SVGB
        // form. Inflating first keeps the SVGB decoder from choking on the wrapper.
        std::vector<std::uint8_t> inflated;

        if (decompress_gzip(payload, payload_size, inflated)) {
            return out.write(inflated.data(), inflated.size()) == inflated.size();
        }

        common::ro_buf_stream in(payload, payload_size);
        std::vector<svgb_convert_error_description> errors;

        if (convert_svgb_to_svg(in, out, errors)) {
            return true;
        }

        if (!errors.empty() && (errors[0].reason_ == svgb_convert_error_invalid_file)) {
            // Rejected at the magic check, so nothing was written yet: the payload is
            // plain SVG text that never went through the binariser.
            return out.write(payload, payload_size) == payload_size;
        }

        return false;
    }
}
