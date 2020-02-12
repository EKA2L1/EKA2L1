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

#pragma once

#include <epoc/services/fbs/bitmap.h>
#include <epoc/services/fbs/font.h>
#include <epoc/services/fbs/font_atlas.h>
#include <epoc/services/fbs/font_store.h>
#include <epoc/services/framework.h>
#include <epoc/services/fbs/adapter/font_adapter.h>
#include <epoc/services/window/common.h>

#include <common/allocator.h>
#include <common/hash.h>

#include <drivers/graphics/common.h>

#include <atomic>
#include <memory>
#include <optional>
#include <unordered_map>

namespace eka2l1 {
    struct file;
    using symfile = std::unique_ptr<file>;

    namespace drivers {
        class graphics_driver;
    }

    namespace epoc {    
        // Before and in build 94, the multiple memory model still make it possible to directly return pointer, since
        // chunk address don't change with each process.
        // But since build 95, the flexible memory model comes in with multi-core, and chunk address may be different
        // with each process, so they use offset instead. This makes a huge change in FBS.
        // They should probably use offset from the beginning.
        constexpr std::uint16_t RETURN_POINTER_NOT_OFFSET_BUILD_LIMIT = 94;
        
        bool does_client_use_pointer_instead_of_offset(fbscli *cli);
    }
    
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
        fbs_mess_unused1,
        fbs_set_heap_fail,
        fbs_heap_count,
        fbs_set_heap_reset,
        fbs_set_heap_check,
        fbs_heap,
        fbs_mess_unused2,
        fbs_bitmap_clean,
        fbs_bitmap_load_fast,
        fbs_bitmap_notify_dirty,
        fbs_bitmap_cancel_notify_dirty,
        fbs_register_linked_typeface,
        fbs_fetch_linked_typeface,
        fbs_set_duplicate_fail,
        fbs_update_linked_typeface,
        fbs_get_font_table,
        fbs_release_font_table,
        fbs_get_glyph_outline,
        fbs_release_glyph_outline,
        fbs_get_glyphs,
        fbs_no_op,
        fbs_get_glyph_metrics,
        fbs_atlas_font_count,
        fbs_atlas_glyph_count,
        fbs_oogm_notification,
        fbs_get_glyph_cache_metrics
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
    struct fbscli;

    struct fbs_dirty_notify_request {
        fbscli *client;
        epoc::notify_info nof;

        bool dirty { false };
    };

    struct fbsfont;

    struct fbscli : public service::typical_session {
        service::uid connection_id_ {0};
        fbs_dirty_notify_request *nof_;
        bool support_dirty_bitmap { true };

        explicit fbscli(service::typical_server *serv, const std::uint32_t ss_id, epoc::version client_version);

        ~fbscli();

        void get_nearest_font(service::ipc_context *ctx);
        void load_bitmap(service::ipc_context *ctx);
        void load_bitmap_fast(service::ipc_context *ctx);
        void get_face_attrib(service::ipc_context *ctx);
        void duplicate_font(service::ipc_context *ctx);
        void duplicate_bitmap(service::ipc_context *ctx);
        void create_bitmap(service::ipc_context *ctx);
        void notify_dirty_bitmap(service::ipc_context *ctx);
        void cancel_notify_dirty_bitmap(service::ipc_context *ctx);
        void get_clean_bitmap(service::ipc_context *ctx);
        void rasterize_glyph(service::ipc_context *ctx);
        void background_compress_bitmap(service::ipc_context *ctx);
        
        void load_bitmap_impl(service::ipc_context *ctx, file *source);
        
        void fetch(service::ipc_context *ctx) override;

        fbsfont *get_font_object(service::ipc_context *ctx);
    };

    struct fbsfont : fbsobj {
        eka2l1::ptr<epoc::bitmapfont> guest_font_handle;
        epoc::open_font_info of_info;
        fbs_server *serv;

        epoc::font_atlas atlas;

        explicit fbsfont()
            : fbsobj(fbsobj_kind::font) {
        }

        void deref() override;
    };

    struct fbsbitmap: public fbsobj {
        epoc::bitwise_bitmap *bitmap_;
        fbs_server *serv_;
        bool shared_ { false };
        fbsbitmap *clean_bitmap;
        bool support_dirty_bitmap;

        explicit fbsbitmap(fbs_server *srv, epoc::bitwise_bitmap *bitmap, const bool shared, const bool support_dirty_bitmap)
            : fbsobj(fbsobj_kind::bitmap)
            , bitmap_(bitmap)
            , serv_(srv)
            , shared_(shared)
            , clean_bitmap(nullptr)
            , support_dirty_bitmap(support_dirty_bitmap) {
        }

        ~fbsbitmap();
    };

    struct fbsbitmap_cache_info {
        std::u16string path;
        int bitmap_idx;
    };

    inline bool operator == (const fbsbitmap_cache_info &lhs, const fbsbitmap_cache_info &rhs) {
        return (lhs.path == rhs.path) && (lhs.bitmap_idx == rhs.bitmap_idx);
    }
}

namespace std {
    template <> struct hash<eka2l1::fbsbitmap_cache_info> {
        std::size_t operator()(eka2l1::fbsbitmap_cache_info const& info) const noexcept {
            std::size_t seed = 0x151A5151;

            eka2l1::common::hash_combine(seed, info.path);
            eka2l1::common::hash_combine(seed, info.bitmap_idx);
            
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
        friend struct fbsfont;

        server_ptr fs_server;

        chunk_ptr shared_chunk;
        chunk_ptr large_chunk;

        std::uint8_t *base_shared_chunk;
        std::uint8_t *base_large_chunk;

        eka2l1::ptr<void> bmp_font_vtab;

        std::u16string default_system_font;
        std::vector<fbs_dirty_notify_request> dirty_nofs;

        std::unordered_map<fbsbitmap_cache_info, fbsbitmap*> shared_bitmaps;

        std::unique_ptr<fbs_chunk_allocator> shared_chunk_allocator;
        std::unique_ptr<fbs_chunk_allocator> large_chunk_allocator;
        
        epoc::open_font_session_cache_list *session_cache_list;
        epoc::open_font_session_cache_link *session_cache_link;

        epoc::font_store persistent_font_store;

        void load_fonts(eka2l1::io_system *io);

        std::atomic<service::uid> connection_id_counter { 0x1234 }; // Easier to debug

        service::normal_object_container font_obj_container;    ///< Specifically storing fonts

    public:
        explicit fbs_server(eka2l1::system *sys);
        ~fbs_server() override;
        
        service::uid init();

        void connect(service::ipc_context &context) override;

        /**
         * \brief  Create a new empty bitmap.
         * 
         * \param  size           Size of the bitmap, in pixels.
         * \param  dpm            Bit per pixels as display mode.
         * \param  support_dirty  True if this bitmap supports clean variant. For backwards compability.
         * 
         * \returns Bitmap object. The ID of bitmap is the server handle.
         * 
         * \see    free_bitmap
         */
        fbsbitmap *create_bitmap(const eka2l1::vec2 &size, const epoc::display_mode dpm, const bool support_dirty = true);
        
        /**
         * \brief   Free a bitmap object.
         * 
         * The function frees bitmap pixels and allocated object from the server's heap.
         * 
         * It will fail if the object is still being referenced by some forces, meaning, if the object
         * reference count is bigger than 0, the function will fail.
         * 
         * \param   bmp The server bitmap object.
         * \returns True if success.
         * 
         * \see     create_bitmap
         */
        bool free_bitmap(fbsbitmap *bmp);

        drivers::graphics_driver *get_graphics_driver();
        
        fbsfont *look_for_font_with_address(const eka2l1::address addr);

        std::uint8_t *get_shared_chunk_base() {
            return base_shared_chunk;
        }

        std::uint8_t *get_large_chunk_base() {
            return base_large_chunk;
        }

        std::uint8_t *get_large_chunk_pointer(const std::uint64_t start_offset) {
            return base_large_chunk + start_offset;
        }

        ptr<std::uint8_t> host_ptr_to_guest_general_data(void *ptr) {
            return shared_chunk->base() + static_cast<std::uint32_t>(reinterpret_cast<std::uint8_t *>(ptr) - base_shared_chunk);
        }

        std::int32_t host_ptr_to_guest_shared_offset(void *ptr) {
            return static_cast<std::int32_t>(reinterpret_cast<std::uint8_t *>(ptr) - base_shared_chunk);
        }

        /**
         * \brief Load compressed image data to large chunk.
         * 
         * \param mbmf_ The MBM file stream
         * \param idx_ Index of the bitmap in MBM. Index base is 0.
         * \param err_code Pointer to integer whic will holds error code. Must not be null.
         * 
         * \return Starting offset from the large chunk.
         */
        std::optional<std::size_t> load_data_to_rom(loader::mbm_file &mbmf_, 
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

        /**
         * \brief Use to allocate large data such as bitmap (font bitmap, raw bitmap, etc...).
         * 
         * \param     s Size of data to be allocated.
         * \returns   Pointer to the large data if memory is efficient.   
         */
        void *allocate_large_data(const std::size_t s);

        /**
         * \brief    Free the large data pointer.
         * \returns  True on success.
         */
        bool free_large_data(const void *ptr);

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

        /**
         * \brief   Get a font by its ID.
         * 
         * \param   id The ID of the font.
         * \returns Pointer to the font object if it exists.
         */
        fbsfont *get_font(const service::uid id);

        template <typename T>
        bool free_general_data(const T *dat) {
            return free_general_data_impl(dat);
        }
    };
}
