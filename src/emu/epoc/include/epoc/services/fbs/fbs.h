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

#include <epoc/services/fbs/bitmap.h>
#include <epoc/services/fbs/font.h>
#include <epoc/services/framework.h>
#include <epoc/services/fbs/adapter/font_adapter.h>

#include <common/allocator.h>
#include <common/hash.h>

#include <atomic>
#include <memory>
#include <optional>
#include <unordered_map>

namespace eka2l1 {
    struct file;
    using symfile = std::shared_ptr<file>;
    
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

    struct fbsobj : public epoc::ref_count_object {
        fbsobj_kind kind;

        explicit fbsobj(const fbsobj_kind kind)
            : kind(kind) {
        }
    };

    class fbs_server;

    struct fbscli : public service::typical_session {
        service::uid connection_id_ {0};

        explicit fbscli(service::typical_server *serv, const std::uint32_t ss_id)
            : service::typical_session(serv, ss_id) {
        }

        void get_nearest_font(service::ipc_context *ctx);
        void load_bitmap(service::ipc_context *ctx);
        void get_face_attrib(service::ipc_context *ctx);

        void load_bitmap_impl(service::ipc_context *ctx, symfile source);
        
        void fetch(service::ipc_context *ctx) override;
    };

    struct open_font_info {
        std::size_t idx;

        epoc::open_font_metrics metrics;
        epoc::adapter::font_file_adapter_base *adapter;
        epoc::open_font_face_attrib face_attrib;
    };

    struct fbsfont : fbsobj {
        eka2l1::ptr<epoc::bitmapfont> guest_font_handle;
        open_font_info of_info;

        explicit fbsfont()
            : fbsobj(fbsobj_kind::font) {
        }
    };

    struct fbsbitmap: public fbsobj {
        epoc::bitwise_bitmap *bitmap_;
        bool shared_ { false };

        explicit fbsbitmap(epoc::bitwise_bitmap *bitmap, const bool shared)
            : fbsobj(fbsobj_kind::bitmap), bitmap_(bitmap)
            , shared_(shared) {
        }
    };

    struct fbsbitmap_cache_info {
        std::u16string path;
        int bitmap_idx;

        std::uint64_t last_access_time_since_ad;
    };

    inline bool operator == (const fbsbitmap_cache_info &lhs, const fbsbitmap_cache_info &rhs) {
        return (lhs.path == rhs.path) && (lhs.bitmap_idx == rhs.bitmap_idx) &&
            (lhs.last_access_time_since_ad == rhs.last_access_time_since_ad);
    }
}

namespace std {
    template <> struct hash<eka2l1::fbsbitmap_cache_info> {
        std::size_t operator()(eka2l1::fbsbitmap_cache_info const& info) const noexcept {
            std::size_t seed = 0x151A5151;

            eka2l1::common::hash_combine(seed, info.path);
            eka2l1::common::hash_combine(seed, info.bitmap_idx);
            eka2l1::common::hash_combine(seed, info.last_access_time_since_ad);
            
            return seed;
        }
    };
}

namespace eka2l1 {
    class io_system;

    enum fbs_load_data_err {
        fbs_load_data_err_none,
        fbs_load_data_err_out_of_mem,
        fbs_load_data_err_read_decomp_fail
    };

    class fbs_chunk_allocator : public common::block_allocator {
        chunk_ptr target_chunk;

    public:
        explicit fbs_chunk_allocator(chunk_ptr de_chunk, std::uint8_t *ptr);
        virtual bool expand(std::size_t target) override;
    };

    class fbs_server : public service::typical_server {
        friend struct fbscli;

        server_ptr fs_server;

        chunk_ptr shared_chunk;
        chunk_ptr large_chunk;

        std::uint8_t *base_shared_chunk;
        std::uint8_t *base_large_chunk;

        std::u16string default_system_font;

        std::unordered_map<std::u16string, std::vector<open_font_info>> open_font_store;
        std::vector<fbsfont*> font_cache;

        std::vector<epoc::adapter::font_file_adapter_instance> font_adapters;

        std::unordered_map<fbsbitmap_cache_info, fbsbitmap*> shared_bitmaps;

        std::unique_ptr<fbs_chunk_allocator> shared_chunk_allocator;
        std::unique_ptr<fbs_chunk_allocator> large_chunk_allocator;

        void load_fonts(eka2l1::io_system *io);

        std::atomic<service::uid> connection_id_counter {1};

    protected:
        void folder_change_callback(eka2l1::io_system *sys, const std::u16string &path, int action);
        void add_fonts_from_adapter(epoc::adapter::font_file_adapter_instance &adapter);

        fbsfont *search_for_cache(epoc::font_spec &spec, const std::uint32_t desired_max_height);
        open_font_info *seek_the_open_font(epoc::font_spec &spec);

    public:
        explicit fbs_server(eka2l1::system *sys);
        service::uid init();

        void connect(service::ipc_context &context) override;        

        std::uint8_t *get_shared_chunk_base() {
            return base_shared_chunk;
        }

        std::uint8_t *get_large_chunk_pointer(const std::uint64_t start_offset) {
            return base_large_chunk + start_offset;
        }

        ptr<std::uint8_t> host_ptr_to_guest_general_data(void *ptr) {
            return shared_chunk->base() + static_cast<std::uint32_t>(reinterpret_cast<std::uint8_t *>(ptr) - base_shared_chunk);
        }

        std::uint32_t host_ptr_to_guest_shared_offset(void *ptr) {
            return static_cast<std::uint32_t>(reinterpret_cast<std::uint8_t *>(ptr) - base_shared_chunk);
        }

        /**
         * \brief Load uncompressed image data to large chunk.
         * 
         * \param mbmf_ The MBM file stream
         * \param idx_ Index of the bitmap in MBM. Index base is 0.
         * \param err_code Pointer to integer whic will holds error code. Must not be null.
         * 
         * \return Starting offset from the large chunk.
         */
        std::optional<std::size_t> load_uncomp_data_to_rom(loader::mbm_file &mbmf_, 
            const std::size_t idx_, int *err_code);

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
