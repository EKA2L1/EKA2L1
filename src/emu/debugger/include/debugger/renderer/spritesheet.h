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

#include <drivers/graphics/common.h>
#include <common/vecx.h>

#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1::drivers {
    class graphics_driver;
    class graphics_command_list_builder;
}

namespace eka2l1::renderer {
    // Please forgive me... 
    struct spritesheet {
    private:
        /**
         * \brief       Parse file contains animation metadata (frame position, size, etc...).
         * 
         * The file should be in XML format.
         * 
         * \param       path The path to meta file.
         * \returns     True on success.
         * 
         * \internal
         */
        bool parse_metadata(const std::string &path);

    public:
        struct frame_metadata {
            float         frame_time_;
            eka2l1::vec2  position_;
            eka2l1::vec2  size_;
            std::string   frame_name_;
        };

        float remaining_;
        int current_;
        int width_;
        int height_;

        drivers::handle sheet_;
        std::vector<frame_metadata> metas_;

        explicit spritesheet();

        bool load(drivers::graphics_driver *driver, drivers::graphics_command_list_builder *builder,
            const std::string &texture_filename, const std::string &meta_filename, const int fps);
        void play(const float elapsed);
        void get_current_frame_uv_coords(float &uv_x_min, float &uv_x_max, float &uv_y_min, float &uv_y_max);
    }; 
}