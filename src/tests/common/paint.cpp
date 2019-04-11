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

#include <common/paint.h>
#include <common/buffer.h>
#include <Catch2/catch.hpp>

#include <fstream>
#include <string>

using namespace eka2l1;

TEST_CASE("simple_line", "painter") {
    common::wo_std_file_stream std_fstream_paint("testpaint.bmp");
    common::buffer_24bmp_pixel_plotter plotter;
    common::painter artist(&plotter);

    artist.new_art({ 240, 320 });
    artist.set_brush_thickness(5);
    artist.set_brush_color({0, 191, 255, 0});

    // Draw a rectangle
    artist.vertical_line({ 10, 5 }, 20 );
    artist.horizontal_line({ 10, 25 }, 160);
    artist.vertical_line({ 170, 5 }, 20);
    artist.horizontal_line({ 10, 5 }, 160);
    artist.line_from_to({ 10, 5 }, { 170, 22 });

    artist.set_brush_color({ 236, 100, 75, 0 });
    artist.flood({ 156, 12 });

    artist.set_brush_thickness(16);
    artist.set_fill_when_draw(true);
    artist.set_brush_color({ 123, 239, 178, 0 });
    artist.rect(eka2l1::rect { {120, 260}, {40, 40} });

    plotter.save_to_bmp(reinterpret_cast<common::wo_stream*>(&std_fstream_paint));
}
