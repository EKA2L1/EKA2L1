/*
 * Copyright (c) 2018 EKA2L1 Team.
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
#pragma once

#include <common/vecx.h>

#include <drivers/screen_driver.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <map>

namespace eka2l1 {
    namespace driver {
        struct ttf_char {
            unsigned int tex_id;
            unsigned int advance;
            vec2 size;
            vec2 bearing;
        };

        class screen_driver_ogl: public screen_driver {
            object_size ssize, fsize;
            FT_Library ft;
            FT_Face face;

            unsigned int fbo;
            unsigned int rbo;
            unsigned int fbt;

            unsigned int quad_vao, quad_vbo;
            unsigned int render_program;
            unsigned int font_program;

            unsigned int dynquad_vao, dynquad_vbo;

            std::map<char, ttf_char> ttf_chars;

            glm::mat4 ortho;

            void init_font();

        public:
            screen_driver_ogl() {}

            eka2l1::vec2 get_window_size() override {
                return ssize;
            }

            void init(emu_window_ptr win, object_size &screen_size, object_size &font_size) override;
            void shutdown() override;

            void blit(const std::string &text, point &where) override;
            bool scroll_up(rect &trect) override;

            void clear(rect &trect) override;

            void set_pixel(const point &pos, uint8_t color) override;
            int get_pixel(const point &pos) override;

            void set_word(const point &pos, int word) override;
            int get_word(const point &pos) override;

            void set_line(const point &pos, const pixel_line &pl) override;
            void get_line(const point &pos, pixel_line &pl) override;

            void begin_render() override;
            void end_render() override;
        };
    }
}
