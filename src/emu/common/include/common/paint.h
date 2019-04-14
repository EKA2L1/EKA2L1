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

#pragma once

#include <common/vecx.h>
#include <vector>

namespace eka2l1::common {
    class wo_stream;

    /**
     * \brief Basic interface for pixel plotter.
     */
    class pixel_plotter {
    public:
        /**
         * \brief Resize the bitmap the plotter handle to targeted size.
         * \param size A vector2 contains the width and height of the new bitmap.
         */
        virtual void resize(const eka2l1::vec2 &size) = 0;

        /**
         * \brief Set pixel at given coordinate.
         * 
         * \param pos Coordinate of the pixel.
         * \param color The color to set.
         */
        virtual void plot_pixel(const eka2l1::vec2 &pos, const eka2l1::vecx<int, 4> &color) = 0;

        /**
         * \brief Get pixel at given index.
         * \param pos Coordinate of the pixel.
         * 
         * \returns A vector-4 contains the color of the pixel.
         */
        virtual eka2l1::vecx<int, 4> get_pixel(const eka2l1::vec2 &pos) = 0;

        /**
         * \brief Get size of the bitmap this plotter handle.
         */
        virtual eka2l1::vec2 &get_size() = 0;
    };

    /**
     * \brief A basic 24-bit bitmap pixel plotter.
     * 
     * This plotter only stores R,G and B channel, and ignore the alpha channel.
     * This plotter produces 24 bits per pixel bitmap.
     */
    class buffer_24bmp_pixel_plotter: public pixel_plotter {
        eka2l1::vec2 size_;
        std::vector<std::uint8_t> buf_;

        int aligned_row_size_in_bytes;

    public:
        void resize(const eka2l1::vec2 &size) override;
        void plot_pixel(const eka2l1::vec2 &pos, const eka2l1::vecx<int, 4> &color) override;
        eka2l1::vecx<int, 4> get_pixel(const eka2l1::vec2 &pos) override;

        eka2l1::vec2 &get_size() override {
            return size_;
        }

        /**
         * \brief Set the content in the bitmap buffer to a stream.
         * \param stream Write-only stream that will contains BMP file data.
         */
        void save_to_bmp(wo_stream *stream);
    };

    class painter {
        pixel_plotter *plotter_;

        eka2l1::vecx<int, 4> brush_col_;
        eka2l1::vecx<int, 4> fill_col_;

        int brush_thick_ { 1 };
        int trans_ { 0 };

        enum {
            PAINTER_FLAG_FILL_WHEN_DRAW = 0x1
        };

        std::uint32_t flags { 0 };

    public:
        explicit painter(pixel_plotter *plotter);

        void set_brush_color(const eka2l1::vecx<int, 4> &color) {
            brush_col_ = color;
        }
        
        void set_fill_color(const eka2l1::vecx<int, 4> &color) {
            fill_col_ = color;
        }

        void set_brush_thickness(const int thickness) {
            brush_thick_ = thickness;
        }

        void set_fill_when_draw(const bool op) {
            flags &= ~PAINTER_FLAG_FILL_WHEN_DRAW;

            if (op) {
                flags |= PAINTER_FLAG_FILL_WHEN_DRAW;
            }
        }

        /**
         * \brief Start a drawing session.
         * 
         * The function resizes the plotter, and then clear the canavas.
         * 
         * \param size The size of the new canavas.
         */
        void new_art(const eka2l1::vec2 &size);

        /**
         * \brief Draws a line.
         * 
         * \param start The position to start drawing the line from.
         * \param end   The position that the line ends.
         */
        void line_from_to(const eka2l1::vec2 &start, const eka2l1::vec2 &end);

        /**
         * \brief Draw a line with given (X, Y) length.
         * 
         * \param start     The position to start drawing the line from.
         * \param direction The length of the line in both X axis and Y axis.
         */
        void line(const eka2l1::vec2 &start, const eka2l1::vec2 &direction) {
            line_from_to(start, start + direction);
        }

        void horizontal_line(const eka2l1::vec2 &start, const int len) {
            line(start, eka2l1::vec2(len, 0));
        }

        void vertical_line(const eka2l1::vec2 &start, const int len) {
            line(start, eka2l1::vec2(0, len));
        }

        /**
         * \brief Draw an ellipse.
         *
         * \param pos The origin of the ellipse.
         * \param rad (X, Y) radius of the ellipse.
         */
        void ellipse(const eka2l1::vec2 &pos, const eka2l1::vec2 &rad);
        
        /**
         * \brief Draw an ellipse with one-pixel thick.
         * 
         * \param pos The origin of the ellipse.
         * \param rad (X, Y) radius of the ellipse.
         */
        void ellipse_one_pix(const eka2l1::vec2 &pos, const eka2l1::vec2 &rad);

        void circle_one_pix(const eka2l1::vec2 &pos, const int radius);

        void circle(const eka2l1::vec2 &pos, const int radius);

        /**
         * \brief Draws a rectangle.
         * 
         * \param re The rectangle to draw.
         */
        void rect(const eka2l1::rect &re);
        
        /**
         * \brief Flood-fill a region with scan-line algorithm.
         * 
         * \param pos       The position to start the fill.
         * \param fill_mode If this is set to true, the flood will stop until encouters pixels
         *                  that have the same color as the brush. Else it will stop until encounters
         *                  pixels that have the same color as the color of the pixel that starts the flood.
         */
        void flood(const eka2l1::vec2 &pos, const bool fill_mode = false);
    };
}
