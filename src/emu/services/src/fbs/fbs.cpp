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

#include <system/epoc.h>
#include <kernel/kernel.h>
#include <kernel/chunk.h>
#include <kernel/libmanager.h>

#include <services/fbs/fbs.h>
#include <services/fs/std.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/thread.h>
#include <common/vecx.h>

#include <utils/cppabi.h>
#include <utils/err.h>

#include <vfs/vfs.h>

#include <config/config.h>

namespace eka2l1 {
    namespace epoc {
        bool does_client_use_pointer_instead_of_offset(fbscli *cli) {
            const epocver current_sys_ver = cli->server<fbs_server>()->get_system()->get_symbian_version_use();
            return (cli->client_version().build <= epoc::RETURN_POINTER_NOT_OFFSET_BUILD_LIMIT) && (current_sys_ver < epocver::epoc95);
        }
            
        std::string get_fbs_server_name_by_epocver(const epocver ver) {
            if (ver < epocver::epoc81a) {
                return "Fontbitmapserver";
            }

            return "!Fontbitmapserver";
        }
    }

    fbscli::~fbscli() {
        // Remove notification if there is
        if (server<fbs_server>()->compressor && !dirty_nof_.empty()) {
            server<fbs_server>()->compressor->cancel(dirty_nof_);
        }

        // Try to remove the session cache if it exists
        if (epoc::does_client_use_pointer_instead_of_offset(this)) {
            // Free in the linked list
            server<fbs_server>()->session_cache_link->remove(this);
        } else {
            server<fbs_server>()->session_cache_list->erase_cache(this);
        }

        if (glyph_info_for_legacy_return_) {
            server<fbs_server>()->free_general_data(glyph_info_for_legacy_return_);
        }
    }

    void fbscli::set_pixel_size_in_twips(service::ipc_context *ctx) {
        server<fbs_server>()->set_pixel_size_in_twips({ ctx->get_argument_value<std::int32_t>(0).value(),
            ctx->get_argument_value<std::int32_t>(1).value() });

        ctx->complete(epoc::error_none);
    }

    void fbscli::fetch(service::ipc_context *ctx) {
        if (ctx->sys->get_symbian_version_use() < epocver::eka2) {
            // Move get nearest font to be after set pixel size in twips
            switch (ctx->msg->function) {
            case fbs_set_pixel_size_in_twips + 1:
                ctx->msg->function = fbs_nearest_font_design_height_in_twips;
                break;

            case fbs_set_pixel_size_in_twips + 2:
                ctx->msg->function = fbs_nearest_font_design_height_in_pixels;
                break;

            default: {
                if ((ctx->msg->function > fbs_set_pixel_size_in_twips + 2) && 
                    (ctx->msg->function < fbs_nearest_font_design_height_in_twips + 2)) {
                    ctx->msg->function -= 2;
                }

                break;
            }
            }

            if (ctx->msg->function == fbs_bitmap_load) {
                // On EKA1 load = fast
                ctx->msg->function = fbs_bitmap_load_fast;
            }
        }

        switch (ctx->msg->function) {
        case fbs_init: {
            connection_id_ = server<fbs_server>()->init();

            if (ctx->sys->get_symbian_version_use() <= epocver::epoc93) {
                connection_id_ = ctx->msg->msg_session->get_associated_handle();
                ctx->complete(epoc::error_none);
            } else {
                ctx->complete(connection_id_);
            }

            break;
        }

        case fbs_num_typefaces: {
            num_typefaces(ctx);
            break;
        }
                              
        case fbs_typeface_support: {
            typeface_support(ctx);
            break;
        }

        case fbs_nearest_font_design_height_in_pixels:
        case fbs_nearest_font_design_height_in_twips:
        case fbs_nearest_font_max_height_in_pixels:
        case fbs_nearest_font_max_height_in_twips: {
            get_nearest_font(ctx);
            break;
        }

        case fbs_get_font_by_id:
            get_font_by_uid(ctx);
            break;

        case fbs_face_attrib: {
            get_face_attrib(ctx);
            break;
        }

        case fbs_get_twips_height: {
            get_twips_height(ctx);
            break;
        }

        case fbs_rasterize: {
            rasterize_glyph(ctx);
            break;
        }

        case fbs_bitmap_load: {
            load_bitmap(ctx);
            break;
        }

        case fbs_bitmap_load_fast: {
            load_bitmap_fast(ctx);
            break;
        }

        case fbs_font_dup: {
            duplicate_font(ctx);
            break;
        }

        case fbs_bitmap_dup: {
            duplicate_bitmap(ctx);
            break;
        }

        case fbs_bitmap_create: {
            create_bitmap(ctx);
            break;
        }

        case fbs_bitmap_resize: {
            resize_bitmap(ctx);
            break;
        }

        case fbs_bitmap_notify_dirty: {
            notify_dirty_bitmap(ctx);
            break;
        }

        case fbs_bitmap_cancel_notify_dirty: {
            cancel_notify_dirty_bitmap(ctx);
            break;
        }

        case fbs_bitmap_clean: {
            get_clean_bitmap(ctx);
            break;
        }

        case fbs_close: {
            if (!obj_table_.remove(*ctx->get_argument_value<std::uint32_t>(0))) {
                ctx->complete(epoc::error_bad_handle);
                break;
            }

            ctx->complete(epoc::error_none);
            break;
        }

        case fbs_bitmap_bg_compress: {
            //LOG_WARN(SERVICE_FBS, "BitmapBgCompress stubbed with 0");
            //ctx->complete(epoc::error_none);
            background_compress_bitmap(ctx);
            break;
        }

        case fbs_bitmap_compress:
            compress_bitmap(ctx);
            break;

        case fbs_set_pixel_size_in_twips:
            set_pixel_size_in_twips(ctx);
            break;

        default: {
            LOG_ERROR(SERVICE_FBS, "Unhandled FBScli opcode 0x{:X}", ctx->msg->function);
            break;
        }
        }
    }

    fbs_server::fbs_server(eka2l1::system *sys)
        : service::typical_server(sys, epoc::get_fbs_server_name_by_epocver(sys->get_symbian_version_use()))
        , persistent_font_store(sys->get_io_system())
        , shared_chunk(nullptr)
        , large_chunk(nullptr)
        , fntstr_seg(nullptr)
        , bmp_font_vtab(0) {
    }

    static void compressor_thread_func(compress_queue *queue) {
        common::set_thread_name("FBS Server compressor thread");
        queue->run();
    }

    int fbs_server::legacy_level() const {
        if (kern->get_epoc_version() <= epocver::epoc6) {
            return FBS_LEGACY_LEVEL_S60V1;
        }

        if (kern->get_epoc_version() <= epocver::epoc81b) {
            return FBS_LEGACY_LEVEL_KERNEL_TRANSITION;
        }

        if (large_bitmap_access_mutex->get_access_count() > 0) {
            return FBS_LEGACY_LEVEL_EARLY_EKA2;
        }

        return FBS_LEGACY_LEVEL_MORDEN;
    }

    void fbs_server::initialize_server() {
        // Initialize those chunks
        shared_chunk = kern->create_and_add<kernel::chunk>(
                                kernel::owner_type::kernel,
                                kern->get_memory_system(),
                                nullptr,
                                "FbsSharedChunk",
                                0,
                                0x10000,
                                0x200000,
                                prot_read_write,
                                kernel::chunk_type::normal,
                                kernel::chunk_access::global,
                                kernel::chunk_attrib::none)
                            .second;

        large_chunk = kern->create_and_add<kernel::chunk>(
                                kernel::owner_type::kernel,
                                kern->get_memory_system(),
                                nullptr,
                                "FbsLargeChunk",
                                0,
                                0,
                                0x2000000,
                                prot_read_write,
                                kernel::chunk_type::normal,
                                kernel::chunk_access::global,
                                kernel::chunk_attrib::none)
                            .second;

        if (!shared_chunk || !large_chunk) {
            LOG_CRITICAL(SERVICE_FBS, "Can't create shared chunk and large chunk of FBS, exiting");
            return;
        }

        if (kern->is_eka1()) {
            large_bitmap_access_mutex = reinterpret_cast<mutex_ptr>(kern->create<kernel::legacy::mutex>(
                "FbsLargeBitmapAccess", kernel::access_type::global_access));
        } else {
            large_bitmap_access_mutex = kern->create<kernel::mutex>(kern->get_ntimer(),
                "FbsLargeBitmapAccess", false, kernel::access_type::global_access);
        }

        if (!large_bitmap_access_mutex) {
            LOG_WARN(SERVICE_FBS, "Large bitmap access mutex fail to create!");
        }

        memory_system *mem = sys->get_memory_system();

        base_shared_chunk = reinterpret_cast<std::uint8_t *>(shared_chunk->host_base());
        base_large_chunk = reinterpret_cast<std::uint8_t *>(large_chunk->host_base());

        shared_chunk_allocator = std::make_unique<epoc::chunk_allocator>(shared_chunk);
        large_chunk_allocator = std::make_unique<epoc::chunk_allocator>(large_chunk);

        if (fntstr_seg = sys->get_lib_manager()->load(u"fntstr.dll")) {
            // _ZTV11CBitmapFont @ 97 NONAME ; #<VT>#
            // Skip the filler (vtable start address) and the typeinfo
            if (kern->is_eka1()) {
                std::uint8_t *addr = nullptr;
                fntstr_seg->get_code_run_addr(nullptr, &addr);

                utils::cpp_gcc98_abi_analyser analyser(addr, fntstr_seg->get_text_size());

                // We got two clues. TypeUid__C11CBitmapFont @ 52 NONAME ; Which is a virtual method (not sure why it's exported)
                // Also this         TextWidthInPixels__C11CBitmapFontRC7TDesC16 @ 51 NONAME    ;
                std::vector<address> clues;
                clues.push_back(fntstr_seg->lookup_no_relocate(51));
                clues.push_back(fntstr_seg->lookup_no_relocate(52));

                bmp_font_vtab = static_cast<std::uint32_t>(analyser.search_vtable(clues));
            } else {
                bmp_font_vtab = fntstr_seg->lookup_no_relocate(97) + 2 * 4;
            }

            if (bmp_font_vtab == 0) {
                LOG_ERROR(SERVICE_FBS, "Unable to find vtable address of CBitmapFont!");
            }
            
            if (kern->is_eka1()) {
                // For relocate later
                bmp_font_vtab += fntstr_seg->get_code_base();
            }
        }

        // Probably also indicates that font aren't loaded yet
        load_fonts(sys->get_io_system());

        fs_server = kern->get_by_name<service::server>(epoc::fs::get_server_name_through_epocver(
            kern->get_epoc_version()));

        // Create session cache list
        session_cache_list = allocate_general_data<epoc::open_font_session_cache_list>();
        session_cache_link = allocate_general_data<epoc::open_font_session_cache_link>();
        session_cache_list->init();

        // Alloc 4 bytes of padding, so the offset 0 never exist. 0 is always a check if data
        // is available.
        large_chunk_allocator->allocate(4);
        shared_chunk_allocator->allocate(4);

        // Create compressor thread
        if (sys->get_config()->fbs_enable_compression_queue) {
            compressor = std::make_unique<compress_queue>(this);
            compressor_thread = std::make_unique<std::thread>(compressor_thread_func, compressor.get());
        }
    }

    void fbs_server::connect(service::ipc_context &context) {
        if (!shared_chunk && !large_chunk) {
            initialize_server();
        }

        // Create new server client
        create_session<fbscli>(&context);
        context.complete(epoc::error_none);
    }

    service::uid fbs_server::init() {
        return ++connection_id_counter;
    }

    void *fbs_server::allocate_general_data_impl(const std::size_t s) {
        if (!shared_chunk || !shared_chunk_allocator) {
            LOG_CRITICAL(SERVICE_FBS, "FBS server hasn't initialized yet");
            return nullptr;
        }

        return shared_chunk_allocator->allocate(s);
    }

    bool fbs_server::free_general_data_impl(const void *ptr) {
        if (!shared_chunk || !shared_chunk_allocator) {
            LOG_CRITICAL(SERVICE_FBS, "FBS server hasn't initialized yet");
            return false;
        }

        return shared_chunk_allocator->free(ptr);
    }

    void *fbs_server::allocate_large_data(const std::size_t s) {
        if (!large_chunk || !large_chunk_allocator) {
            LOG_CRITICAL(SERVICE_FBS, "FBS server hasn't initialized yet");
            return nullptr;
        }

        return large_chunk_allocator->allocate(s);
    }

    bool fbs_server::free_large_data(const void *ptr) {
        if (!large_chunk || !large_chunk_allocator) {
            LOG_CRITICAL(SERVICE_FBS, "FBS server hasn't initialized yet");
            return false;
        }

        return large_chunk_allocator->free(ptr);
    }

    fbs_server::~fbs_server() {
        if (compressor) {
            compressor->abort();
            compressor_thread->join();
        }

        clear_all_sessions();
        
        font_obj_container.clear();
        obj_con.clear();

        // Destroy chunks.
        if (shared_chunk)
            kern->destroy(shared_chunk);

        if (large_chunk)
            kern->destroy(large_chunk);
    }

    drivers::graphics_driver *fbs_server::get_graphics_driver() {
        return sys->get_graphics_driver();
    }

    fbscli::fbscli(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version)
        , glyph_info_for_legacy_return_(nullptr)
        , glyph_info_for_legacy_return_addr_(0) {
        fbs_server *fbss = reinterpret_cast<fbs_server*>(serv);

        if (fbss->legacy_level() >= FBS_LEGACY_LEVEL_KERNEL_TRANSITION) {
            if (fbss->legacy_level() == FBS_LEGACY_LEVEL_KERNEL_TRANSITION)
                support_current_display_mode = true;
            else
                support_current_display_mode = false;

            support_dirty_bitmap = false;

            glyph_info_for_legacy_return_ = fbss->allocate_general_data<epoc::open_font_glyph_v1_use_for_fbs>();
            glyph_info_for_legacy_return_addr_ = fbss->host_ptr_to_guest_general_data(glyph_info_for_legacy_return_).ptr_address();
        }
    }
}
