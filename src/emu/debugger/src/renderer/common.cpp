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

#include <debugger/renderer/common.h>
#include <stb_image.h>

#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>

namespace eka2l1::renderer {
    drivers::handle load_texture_from_file(drivers::graphics_driver *driver, drivers::graphics_command_list_builder *builder,
        const std::string &path, const bool as_bitmap, int *to_width, int *to_height) {
        if (path.empty()) {
            return 0;
        }

        int width = 0;
        int height = 0;
        int comp = 0;

        unsigned char *dat = stbi_load(path.c_str(), &width, &height, &comp, STBI_rgb_alpha);

        if (dat == nullptr) {
            return 0;
        }

        if (to_width) {
            *to_width = width;
        }

        if (to_height) {
            *to_height = height;
        }

        drivers::handle tex = 0;
        
        if (as_bitmap) {
            tex = drivers::create_bitmap(driver, { width, height });
            builder->update_bitmap(tex, 32, reinterpret_cast<const char*>(dat), width * height * 4,
                { 0, 0 }, { width, height });
        } else {
            tex = drivers::create_texture(driver, 2, 0, drivers::texture_format::rgba, drivers::texture_format::rgba,
                drivers::texture_data_type::ubyte, dat, { width, height, 0 });
            builder->set_texture_filter(tex, drivers::filter_option::linear, drivers::filter_option::linear);
        }

        stbi_image_free(dat);

        return tex;
    }    
}