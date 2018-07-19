/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <common/queue.h>

#include <core/services/fast_heap.h>
#include <core/services/server.h>
#include <core/vfs.h>

#include <atomic>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <string>
#include <vector>

namespace eka2l1 {
    class io_system;

    struct font {
        int handle;

        std::string font_name;
        uint32_t attrib;
        FT_Face ft_face;

        std::string font_path;
    };

    class fontbitmap_server : public service::server {
        enum {
            max_font_cache = 30
        };

        bool cache_loaded = false;

        chunk_ptr fbs_shared_chunk;
        fast_heap fbs_heap;

        FT_Library ft_lib;

        eka2l1::cn_queue<font> ft_fonts;

        void init(service::ipc_context ctx);
        void get_nearest_font(service::ipc_context ctx);

        void do_cache_fonts(io_system *io);
        void add_font_to_cache(font &f);
        font *get_cache_font(const std::string &font_path);
        font *get_cache_font_by_family_and_style(const std::string &font_path, uint32_t style);

        /*! \brief Search cache for the font. */
        font *get_font(io_system *io, const std::string &font_name, uint32_t style);

    public:
        fontbitmap_server(system *sys);

        void destroy() override;
    };
}