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

#include <stack>

namespace eka2l1::common {
    void buffer_24bmp_pixel_plotter::resize(const eka2l1::vec2 &size) {
        aligned_row_size_in_bytes = size.x * 3;

        // Align row pixel size
        if (aligned_row_size_in_bytes % 4 != 0) {
            aligned_row_size_in_bytes += 4 - aligned_row_size_in_bytes % 4;
        }

        buf_.resize(aligned_row_size_in_bytes * size.y);
        size_ = size;
    }
    
    void buffer_24bmp_pixel_plotter::plot_pixel(const eka2l1::vec2 &pos, const eka2l1::vecx<int, 4> &color) {
        if (pos.x < 0 || pos.x >= size_.x || pos.y < 0 || pos.y >= size_.y) {
            return;
        }

        // Byte byte transparency :(
        buf_[(aligned_row_size_in_bytes * pos.y) + pos.x * 3] = color[2];
        buf_[(aligned_row_size_in_bytes * pos.y) + pos.x * 3 + 1] = color[1];
        buf_[(aligned_row_size_in_bytes * pos.y) + pos.x * 3 + 2] = color[0];
    }

    eka2l1::vecx<int, 4> buffer_24bmp_pixel_plotter::get_pixel(const eka2l1::vec2 &pos) {
        return { buf_[(aligned_row_size_in_bytes * pos.y) + pos.x * 3 + 2],
                 buf_[(aligned_row_size_in_bytes * pos.y) + pos.x * 3 + 1],
                 buf_[(aligned_row_size_in_bytes * pos.y) + pos.x * 3],
                 255 };
    }

    void buffer_24bmp_pixel_plotter::save_to_bmp(wo_stream *stream) {
        common::bmp_header header;
        header.file_size = static_cast<std::uint32_t>(sizeof(common::bmp_header) + sizeof(common::dib_header_v1) + buf_.size());
        header.pixel_array_offset = header.file_size - static_cast<std::uint32_t>(buf_.size());
    
        common::dib_header_v1 dib_header;
        dib_header.bit_per_pixels = 24;
        dib_header.color_plane_count = 1;
        dib_header.important_color_count = 0;
        dib_header.palette_count = 0;
        dib_header.comp = 0;
        dib_header.size = size_;
        dib_header.uncompressed_size = static_cast<std::uint32_t>(buf_.size());

        dib_header.size.y = -dib_header.size.y;
        dib_header.print_res = dib_header.size * 15;

        stream->write(&header, sizeof(common::bmp_header));
        stream->write(&dib_header, dib_header.header_size);
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
                plotter_->plot_pixel({i, j}, { 255, 255, 255, 255 });
            }
        }
    }

    void painter::flood(const eka2l1::vec2 &pos, const bool fill_mode) {
        // Scanline is being used here
        std::stack<eka2l1::vec2> floody;
        floody.push(pos);

        auto old_color = plotter_->get_pixel(pos);

        if (old_color == brush_col_) {
            return;
        }

        bool span_above = false;
        bool span_below = false;

        const auto psize = plotter_->get_size();

        while (!floody.empty()) {
            eka2l1::vec2 ant_pos = std::move(floody.top());
            floody.pop();

            // Scan for the beginning of our pixels sequence
            while (ant_pos.x >= 0 && fill_mode ? (plotter_->get_pixel(ant_pos) != brush_col_) :
                (plotter_->get_pixel(ant_pos) == old_color)) {
                ant_pos.x--;
            }

            ant_pos.x++;

            if (!fill_mode || (fill_mode && ant_pos.x > 0)) {
                span_above = false;
                span_below = false;

                while (ant_pos.x < psize.x && fill_mode ? (plotter_->get_pixel(ant_pos) != brush_col_) :
                    (plotter_->get_pixel(ant_pos) == old_color)) {
                    // Plot our pixel
                    plotter_->plot_pixel(ant_pos, brush_col_);

                    auto pix = plotter_->get_pixel(ant_pos + vec2(0, -1));

                    // Scanning above. Well, is there a sign of a sequence to be fill ?
                    if (!span_above && ant_pos.y > 0 && (fill_mode ? pix != brush_col_ : pix == old_color)) {
                        // There is. Enable span above, to check for end of sequence
                        floody.push(ant_pos + vec2(0, -1));
                        span_above = true;
                    } else if (span_above && ant_pos.y > 0 && (fill_mode ? pix == brush_col_ : pix != old_color)) {
                        // End of sequeuce, disable span above, check for new sequence to push 
                        span_above = false;
                    }

                    pix = plotter_->get_pixel(ant_pos + vec2(0, 1));

                    // Scanning below. Well, is there a sign of a sequence to be fill ?
                    if (!span_below && ant_pos.y < psize.y - 1 && (fill_mode ? pix != brush_col_ : pix == old_color)) {
                        // There is. Enable span below, to check for end of sequence
                        floody.push(ant_pos + vec2(0, 1));
                        span_below = true;
                    } else if (span_below && ant_pos.y < psize.y - 1 && (fill_mode ? pix == brush_col_ : pix != old_color)) {
                        // End of sequeuce, disable span below, check for new sequence to push
                        span_below = false;
                    }

                    // Increase the plot horizontal pos
                    ant_pos.x++;
                }
            }
        }
    }
       
    void painter::circle_one_pix(const eka2l1::vec2 &pos, const int radius) {
        int x = 0;
        int y = radius;

        auto circular_plot = [&]() {
            plotter_->plot_pixel({ pos.x + x, pos.y + y }, brush_col_);
            plotter_->plot_pixel({ pos.x + x, pos.y - y }, brush_col_);
            plotter_->plot_pixel({ pos.x - x, pos.y + y }, brush_col_);
            plotter_->plot_pixel({ pos.x - x, pos.y - y }, brush_col_);

            plotter_->plot_pixel({ pos.x + y, pos.y + x }, brush_col_);
            plotter_->plot_pixel({ pos.x + y, pos.y - x }, brush_col_);
            plotter_->plot_pixel({ pos.x - y, pos.y + x }, brush_col_);
            plotter_->plot_pixel({ pos.x - y, pos.y - x }, brush_col_);
        };

        circular_plot();

        int d = 3 - 2 * radius; 

        while (y >= x) {
            x++;

            if (d > 0) {
                y--;
                d += 4 * (x - y) + 10;
            } else {
                d += 4 * x + 6;
            }

            circular_plot();
        }
    }
    
    void painter::circle(const eka2l1::vec2 &pos, const int radius) {
        if (flags & PAINTER_FLAG_FILL_WHEN_DRAW) {
            // Hehe
            auto old_color = brush_col_;
            set_brush_color(fill_col_);

            circle_one_pix(pos, radius + brush_thick_);
            flood(pos, true);

            set_brush_color(old_color);
        }

        circle_one_pix(pos, radius);

        if (brush_thick_ > 1) {
            circle_one_pix(pos, radius + brush_thick_);
            flood({ pos.x + radius + 1, pos.y }, true);
        }
    }

    void painter::ellipse(const eka2l1::vec2 &pos, const eka2l1::vec2 &rad) {
        if (flags & PAINTER_FLAG_FILL_WHEN_DRAW) {
            // Hehe
            auto old_color = brush_col_;
            set_brush_color(fill_col_);

            ellipse_one_pix(pos, rad + brush_thick_);
            flood(pos, true);
            
            set_brush_color(old_color);
        }

        ellipse_one_pix(pos, rad);

        if (brush_thick_ > 1) {
            ellipse_one_pix(pos, rad + brush_thick_);
            flood({ pos.x + rad.x + 1, pos.y }, true);
        }
    }
    
    void painter::ellipse_one_pix(const eka2l1::vec2 &pos, const eka2l1::vec2 &rad) {
        float p = (rad.y * rad.y) - (rad.x * rad.x) * (0.25f - rad.y);
        int x = 0;
        int y = rad.y;

        do {
            plotter_->plot_pixel({ pos.x + x, pos.y + y }, brush_col_);
            plotter_->plot_pixel({ pos.x + x, pos.y - y }, brush_col_);
            plotter_->plot_pixel({ pos.x - x, pos.y + y }, brush_col_);
            plotter_->plot_pixel({ pos.x - x, pos.y - y }, brush_col_);

            if (p < 0) {
                x += 1;
                p += rad.y * rad.y * (2 * x + 1);
            } else {
                x += 1;
                y -= 1;
                p += rad.y * rad.y * (2 * x + 1) - 2 * rad.x * rad.x * y;
            }
        } while (2 * rad.y * rad.y * x < 2 * rad.x * rad.x * y);

        // Upper region
        p = (rad.y * rad.y * (x + 0.5f)  * (x + 0.5f)) + (((y - 1) * (y - 1) - rad.y * rad.y) * rad.x * rad.x);
        
        do {
            plotter_->plot_pixel({ pos.x + x, pos.y + y }, brush_col_);
            plotter_->plot_pixel({ pos.x + x, pos.y - y }, brush_col_);
            plotter_->plot_pixel({ pos.x - x, pos.y + y }, brush_col_);
            plotter_->plot_pixel({ pos.x - x, pos.y - y }, brush_col_);

            if (p > 0) {
                y -= 1;
                p -= rad.x * rad.x * (2 * y - 1);
            } else {
                x += 1;
                y -= 1;
                p = p - rad.x * rad.x * (2 * y - 1) + 2 * rad.y * rad.y * x;
            }
        } while (y != 0);

        plotter_->plot_pixel({ pos.x + rad.x, pos.y }, brush_col_);
        plotter_->plot_pixel({ pos.x - rad.x, pos.y }, brush_col_);
    }

    void painter::rect(const eka2l1::rect &re) {
        horizontal_line({ re.top.x - brush_thick_ / 2 + 1 - brush_thick_ % 2, re.top.y }, re.size.x + brush_thick_ / 2 - 1 + brush_thick_ % 2);
        vertical_line(re.top, re.size.y);
        horizontal_line({ re.top.x, re.size.y + re.top.y }, re.size.x);
        vertical_line({ re.top.x + re.size.x, re.top.y }, re.size.y);

        if (flags & PAINTER_FLAG_FILL_WHEN_DRAW) {
            flood(re.top + brush_thick_, true);
        }
    }

    void painter::line_from_to(const eka2l1::vec2 &start, const eka2l1::vec2 &end) {
        // Bresenham Line-Drawing Algorithm
        const vec2 delta = (end - start).abs();
        const int derr = delta.x - delta.y;

        float dlen = (delta.x + delta.y == 0) ? 1 : std::sqrt(static_cast<float>(delta.x * delta.x + 
            delta.y * delta.y));

        int err = derr;

        int sx = start.x < end.x ? 1 : -1;
        int sy = start.y < end.y ? 1 : -1;

        int base_x = start.x;
        int base_y = start.y;

        int base_x_2 = start.x;
        int base_y_2 = start.y;

        int width = brush_thick_;
        int e2 = 0;

        // Keep drawing between
        for (width = (width + 1) / 2 ; ; ) {
            // Plot base pixel first
            plotter_->plot_pixel({ base_x, base_y }, brush_col_/* * static_cast<int>(static_cast<float>(err - derr) / dlen - width + 1)*/);

            e2 = err;
            base_x_2 = base_x;

            if (2 * e2 >= -delta.x) {
                for (e2 += delta.y, base_y_2 = base_y; e2 < dlen * width && (end.y != base_y_2 || delta.x > delta.y)
                    ; e2 += delta.x) {
                    plotter_->plot_pixel({ base_x, base_y_2 += sy }, brush_col_ /** static_cast<int>(static_cast<float>(e2) / dlen - width + 1)*/);
                }

                if (base_x == end.x) {
                    break;
                }

                e2 = err; 
                err -= delta.y; 
                base_x += sx;
            }

            if (2 * e2 <= delta.y) {
                for (e2 = delta.x - e2; e2 < dlen * width && (end.x != base_x_2 || delta.x < delta.y); 
                    e2 += delta.y) {
                    plotter_->plot_pixel({ base_x_2 += sx, base_y }, brush_col_/* * static_cast<int>(static_cast<float>(e2) / dlen - width + 1)*/);
                }

                if (base_y == end.y) {
                    break;
                }

                err += delta.x;
                base_y += sy;
            }
        } 
    }
}
