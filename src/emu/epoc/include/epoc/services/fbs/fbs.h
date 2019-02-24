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

#include <epoc/services/fbs/font.h>
#include <epoc/services/server.h>

#include <common/allocator.h>

#include <atomic>
#include <memory>
#include <unordered_map>

#include <stb_truetype.h>

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
        fbs_user_alloc_fail,
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

    enum class fbsobj_kind {
        font,
        bitmap
    };

    struct fbsobj {
        fbsobj_kind kind;
        std::uint32_t id;

        explicit fbsobj(const std::uint32_t id, const fbsobj_kind kind)
            : id(id)
            , kind(kind) {
        }
    };

    struct fbscli;

    struct fbshandles {
        fbscli *owner;
        std::vector<fbsobj *> objects;

        std::uint32_t make_handle(std::size_t index);
        std::uint32_t add_object(fbsobj *obj);
        bool remove_object(std::size_t index);

        fbsobj *get_object(const std::uint32_t handle);
    };

    class fbs_server;

    struct fbscli {
        std::uint32_t session_id;
        fbshandles handles;
        fbs_server *server;

        explicit fbscli(fbs_server *serv, const std::uint32_t ss_id);

        void get_nearest_font(service::ipc_context *ctx);
        void fetch(service::ipc_context *ctx);
    };

    struct fbsfont : fbsobj {
        // Reuse and recycle
        std::vector<std::uint8_t> data;

        std::unique_ptr<stbtt_fontinfo> stb_handle;
        eka2l1::ptr<epoc::bitmapfont> guest_font_handle;

        explicit fbsfont(const std::uint32_t id)
            : fbsobj(id, fbsobj_kind::font) {
        }
    };

    class io_system;

    class fbs_chunk_allocator : public common::block_allocator {
        chunk_ptr target_chunk;

    public:
        explicit fbs_chunk_allocator(chunk_ptr de_chunk, std::uint8_t *ptr);
        bool expand(std::size_t target) override;
    };

    class fbs_server : public service::server {
        friend struct fbscli;

        chunk_ptr shared_chunk;
        chunk_ptr large_chunk;

        std::uint8_t *base_shared_chunk;
        std::uint8_t *base_large_chunk;

        std::unordered_map<std::uint32_t, fbscli> clients;
        std::vector<fbsfont> font_avails;
        std::vector<fbsfont *> matched;

        std::unique_ptr<fbs_chunk_allocator> shared_chunk_allocator;
        std::unique_ptr<fbs_chunk_allocator> large_chunk_allocator;

        std::atomic<std::uint32_t> id_counter;

        void load_fonts(eka2l1::io_system *io);

    protected:
        void folder_change_callback(eka2l1::io_system *sys, const std::u16string &path, int action);

        fbscli *get_client_associated_with_handle(const std::uint32_t handle);
        fbsobj *get_object(const std::uint32_t handle);

    public:
        explicit fbs_server(eka2l1::system *sys);
        void init(service::ipc_context context);

        void redirect(service::ipc_context context);

        std::uint8_t *get_shared_chunk_base() {
            return base_shared_chunk;
        }

        ptr<std::uint8_t> host_ptr_to_guest_general_data(void *ptr) {
            return shared_chunk->base() + static_cast<std::uint32_t>(reinterpret_cast<std::uint8_t *>(ptr) - base_shared_chunk);
        }

        /*! \brief Use to Allocate structure from server side.
         *
         * Symbian usually avoids sendings struct that usually changes its structure
         * to preserve compability. Especially, struct with vtable should be avoided.
         * 
         * Using a shared global chunk, this could be solved someway.
        */
        void *allocate_general_data_impl(const std::size_t s);

        // General...
        bool free_general_data_impl(const void *ptr);

        /*! \brief Use to Allocate structure from server side.
         *
         * Symbian usually avoids sendings struct that usually changes its structure
         * to preserve compability. Especially, struct with vtable should be avoided.
         * 
         * Using a shared global chunk, this could be solved someway.
        */
        template <typename T>
        T *allocate_general_data() {
            return reinterpret_cast<T *>(allocate_general_data_impl(sizeof(T)));
        }

        template <typename T>
        bool free_general_data(const T *dat) {
            return free_general_data_impl(dat);
        }
    };
}