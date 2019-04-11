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

#include <common/bitmap.h>
#include <common/buffer.h>
#include <common/paint.h>

namespace eka2l1::common {
    void buffer_24bmp_pixel_plotter::resize(const eka2l1::vec2 &size) {
        aligned_row_size_in_bytes = size.x * 3;

        // Align row pixel size
        if (aligned_row_size_in_bytes % 4 != 0) {
            aligned_row_size_in_bytes += 4 - aligned_row_size_in_bytes % 4;
        }

        buf_.resize(aligned_row_size_in_bytes * size.y);
    }
    
    void buffer_24bmp_pixel_plotter::plot_pixel(const eka2l1::vec2 &pos, const eka2l1::vecx<int, 4> &color) {
        // Byte byte transparency :(
        buf_[(aligned_row_size_in_bytes * pos.y) + pos.x * 3] = color[0];
        buf_[(aligned_row_size_in_bytes * pos.y) + pos.x * 3 + 1] = color[1];
        buf_[(aligned_row_size_in_bytes * pos.y) + pos.x * 3 + 2] = color[2];
    }

    void buffer_24bmp_pixel_plotter::save_to_bmp(wo_stream *stream) {
        common::bmp_header header;
        header.file_size = static_cast<std::uint32_t>(sizeof(common::bmp_header) + sizeof(common::dib_header_v1) + buf_.size());
        header.pixel_array_offset = header.file_size - static_cast<std::uint32_t>(buf_.size());
    
        common::dib_header_v1 dib_header;
        dib_header.bit_per_pixels = 24;
        dib_header.color_plane_count = 1;
        dib_header.header_size = sizeof(dib_header_v1);
        dib_header.important_color_count = 0;
        dib_header.palette_count = 0;
        dib_header.size = size_;
        dib_header.uncompressed_size = static_cast<std::uint32_t>(buf_.size());

        stream->write(&header, sizeof(common::bmp_header));
        stream->write(&dib_header, sizeof(common::dib_header_v1));
        stream->write(&buf_[0], static_cast<std::uint32_t>(buf_.size()));
    }
    
    painter::painter(pixel_plotter *plotter)
        : plotter_(plotter) {
    }

    void painter::new_art(const eka2l1::vec2 &size) {
        plotter_->resize(size);

        // Clear the bitmap
        for (int i = 0; i < size.x; i++) {
            for (int j = 0; j < size.y; j++) {
                // Plot empty white transparent pixel
                plotter_->plot_pixel({i, j}, { 0, 0, 0, 255 });
            }
        }
    }

    void painter::line(const eka2l1::vec2 &start, const eka2l1::vec2 &end) {
        for (int i = 0; i < brush_thick_; i++) {
            for (eka2l1::vec2 pix = start; pix < end; pix += 1) {
                plotter_->plot_pixel(pix, brush_col_);
            }
        }
    }
}
