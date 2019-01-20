/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <epoc/services/server.h>
#include <unordered_map>

namespace eka2l1 {
    enum fbs_opcode {
        fbs_init,
        fbs_shutdown,
        fbs_close,
        fbs_resource_count,
        fbs_num_typefaces,
        fbs_typeface_support,
        fbs_font_height_in_twips,
        fbs_font_height_in_pixels,
        fbs_add_font_store_file,
        fbs_install_font_store_file,
        fbs_remove_font_store_file,
        fbs_set_pixel_height,
        fbs_get_font_by_id,
        fbs_font_dup,
        fbs_bitmap_create,
        fbs_bitmap_resize,
        fbs_bitmap_dup,
        fbs_bitmap_load,
        fbs_bitmap_default_alloc_fail,
        fbs_bitmap_default_mark,
        fbs_bitmap_default_mark_end,
        fbs_user_mark,
        fbs_user_mark_end,
        fbs_heap_check,
        fbs_rasterize,
        fbs_face_attrib,
        fbs_has_character,
        fbs_set_default_glyph_bitmap_type,
        fbs_get_default_glyph_bitmap_type,
        fbs_font_name_alias,
        fbs_bitmap_compress,
        fbs_get_heap_sizes,
        fbs_nearest_font_design_height_in_twips,
        fbs_nearest_font_max_height_in_twips,
        fbs_nearest_font_design_height_in_pixels,
        fbs_nearest_font_max_height_in_pixels,
        fbs_shape_text,
        fbs_shape_delete,
        fbs_default_lang_for_metrics,
        fbs_set_twips_height,
        fbs_get_twips_height,
        fbs_compress,
        fbs_bitmap_bg_compress,
        fbs_unused1,
        fbs_set_system_default_typeface_name,
        fbs_get_all_bitmap_handles,
        fbs_mess_unused1
    };

    struct fbscli {

    };

    class fbs_server: public service::server {
        chunk_ptr   shared_chunk;
        chunk_ptr   large_chunk;

        std::unordered_map<std::uint32_t, fbscli> clients;

    public:
        explicit fbs_server(eka2l1::system *sys);

        void init(service::ipc_context context);
    };
}