/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include "scene.h"

#include <glad/glad.h>
#include <common/bitmap.h>
#include <drivers/graphics/graphics.h>

#include <fstream>
#include <vector>

using namespace eka2l1;

int bitmap_upload_and_draw_scene(void *data) {
    bitmap_draw_scene *bmp_scene = &reinterpret_cast<scenes*>(data)->bmp_draw_scene;
    scenes *sc = reinterpret_cast<scenes*>(data);

    if (!bmp_scene->flags & scene_flags::init) {
        std::ifstream fi("smile.bmp", std::ios::binary);
        if (fi.fail()) {
            return -1;
        }

        fi.read(reinterpret_cast<char*>(&bmp_scene->bmp_header), sizeof(common::bmp_header));

        common::dib_header_v1 header_v1;
        fi.read(reinterpret_cast<char*>(&bmp_scene->dib_header), sizeof(common::dib_header_v1));

        // Seek to bmp data.
        fi.seekg(bmp_scene->bmp_header.pixel_array_offset, std::ios::beg);

        std::vector<char> dat;

        dat.resize(bmp_scene->dib_header.uncompressed_size);
        fi.read(&dat[0], dat.size());

        // Now we can get the info from the bitmap
        bmp_scene->tex = sc->driver->upload_bitmap(0, bmp_scene->dib_header.uncompressed_size, bmp_scene->dib_header.size.x,
            bmp_scene->dib_header.size.y, bmp_scene->dib_header.bit_per_pixels, &dat[0]);

        bmp_scene->flags |= scene_flags::init;
    }

    if (sc->gr_api == drivers::graphic_api::opengl) {
        glClearColor(0.2f, 0.4f, 0.6f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // Draw
    sc->spritebatch->draw(bmp_scene->tex, eka2l1::vec2(0, 0), bmp_scene->dib_header.size);
    return true;
}
