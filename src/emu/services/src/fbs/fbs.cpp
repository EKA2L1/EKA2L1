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

#include <epoc/epoc.h>
#include <kernel/kernel.h>
#include <kernel/chunk.h>
#include <kernel/libmanager.h>
#include <services/fbs/fbs.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/thread.h>
#include <common/vecx.h>

#include <utils/err.h>
#include <vfs/vfs.h>

#include <manager/config.h>

namespace eka2l1 {
    namespace epoc { 
        bool does_client_use_pointer_instead_of_offset(fbscli *cli) {
            const epocver current_sys_ver = cli->server<fbs_server>()->get_system()->get_symbian_version_use();
            return (cli->client_version().build <= epoc::RETURN_POINTER_NOT_OFFSET_BUILD_LIMIT) && (current_sys_ver < epocver::epoc95);
        }
    }

    fbs_chunk_allocator::fbs_chunk_allocator(chunk_ptr de_chunk, std::uint8_t *dat_ptr)
        : block_allocator(dat_ptr, de_chunk->committed())
        , target_chunk(std::move(de_chunk)) {
    }

    bool fbs_chunk_allocator::expand(std::size_t target) {
        return target_chunk->adjust(target);
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
    }

    void fbscli::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case fbs_init: {
            connection_id_ = server<fbs_server>()->init();

            if (ctx->sys->get_symbian_version_use() <= epocver::epoc93) {
                connection_id_ = ctx->msg->msg_session->get_associated_handle();
                ctx->set_request_status(epoc::error_none);
            } else {
                ctx->set_request_status(connection_id_);
            }

            break;
        }

        case fbs_num_typefaces: {
            num_typefaces(ctx);
            break;
        }

        case fbs_nearest_font_design_height_in_pixels:
        case fbs_nearest_font_design_height_in_twips: {
            get_nearest_font(ctx);
            break;
        }

        case fbs_face_attrib: {
            get_face_attrib(ctx);
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
            if (!obj_table_.remove(*ctx->get_arg<std::uint32_t>(0))) {
                ctx->set_request_status(epoc::error_bad_handle);
                break;
            }

            ctx->set_request_status(epoc::error_none);
            break;
        }

        case fbs_bitmap_bg_compress: {
            //LOG_WARN("BitmapBgCompress stubbed with 0");
            //ctx->set_request_status(epoc::error_none);
            background_compress_bitmap(ctx);
            break;
        }

        default: {
            LOG_ERROR("Unhandled FBScli opcode 0x{:X}", ctx->msg->function);
            break;
        }
        }
    }

    fbs_server::fbs_server(eka2l1::system *sys)
        : service::typical_server(sys, "!Fontbitmapserver")
        , persistent_font_store(sys->get_io_system())
        , shared_chunk(nullptr)
        , large_chunk(nullptr) {
    }

    static void compressor_thread_func(compress_queue *queue) {
        common::set_thread_name("FBS Server compressor thread");
        queue->run();
    }

    bool fbs_server::legacy_mode() const {
        return large_bitmap_access_mutex->get_access_count() > 0;
    }

    void fbs_server::connect(service::ipc_context &context) {
        if (!shared_chunk && !large_chunk) {
            // Initialize those chunks
            kernel_system *kern = context.sys->get_kernel_system();
            shared_chunk = kern->create_and_add<kernel::chunk>(
                                   kernel::owner_type::kernel,
                                   kern->get_memory_system(),
                                   nullptr,
                                   "FbsSharedChunk",
                                   0,
                                   0x10000,
                                   0x200000,
                                   prot::read_write,
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
                                  prot::read_write,
                                  kernel::chunk_type::normal,
                                  kernel::chunk_access::global,
                                  kernel::chunk_attrib::none)
                              .second;

            if (!shared_chunk || !large_chunk) {
                LOG_CRITICAL("Can't create shared chunk and large chunk of FBS, exiting");
                context.set_request_status(epoc::error_no_memory);

                return;
            }

            large_bitmap_access_mutex = kern->create<kernel::mutex>(kern->get_ntimer(),
                "FbsLargeBitmapAccess", false, kernel::access_type::global_access);

            if (!large_bitmap_access_mutex) {
                LOG_WARN("Large bitmap access mutex fail to create!");
            }

            memory_system *mem = sys->get_memory_system();

            base_shared_chunk = reinterpret_cast<std::uint8_t *>(shared_chunk->host_base());
            base_large_chunk = reinterpret_cast<std::uint8_t *>(large_chunk->host_base());

            shared_chunk_allocator = std::make_unique<fbs_chunk_allocator>(shared_chunk,
                base_shared_chunk);

            large_chunk_allocator = std::make_unique<fbs_chunk_allocator>(large_chunk,
                base_large_chunk);

            if (auto seg = sys->get_lib_manager()->load(u"fntstr.dll", nullptr)) {
                // _ZTV11CBitmapFont @ 97 NONAME ; #<VT>#
                // Skip the filler (vtable start address) and the typeinfo
                bmp_font_vtab = seg->lookup(nullptr, 97) + 2 * 4;
            }

            // Probably also indicates that font aren't loaded yet
            load_fonts(context.sys->get_io_system());

            fs_server = kern->get_by_name<service::server>("!FileServer");

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

        // Create new server client
        create_session<fbscli>(&context);
        context.set_request_status(epoc::error_none);
    }

    service::uid fbs_server::init() {
        return ++connection_id_counter;
    }

    void *fbs_server::allocate_general_data_impl(const std::size_t s) {
        if (!shared_chunk || !shared_chunk_allocator) {
            LOG_CRITICAL("FBS server hasn't initialized yet");
            return nullptr;
        }

        return shared_chunk_allocator->allocate(s);
    }

    bool fbs_server::free_general_data_impl(const void *ptr) {
        if (!shared_chunk || !shared_chunk_allocator) {
            LOG_CRITICAL("FBS server hasn't initialized yet");
            return false;
        }

        return shared_chunk_allocator->free(ptr);
    }

    void *fbs_server::allocate_large_data(const std::size_t s) {
        if (!large_chunk || !large_chunk_allocator) {
            LOG_CRITICAL("FBS server hasn't initialized yet");
            return nullptr;
        }

        return large_chunk_allocator->allocate(s);
    }

    bool fbs_server::free_large_data(const void *ptr) {
        if (!large_chunk || !large_chunk_allocator) {
            LOG_CRITICAL("FBS server hasn't initialized yet");
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

        // Destroy chunks.
        if (shared_chunk)
            kern->destroy(shared_chunk);

        if (large_chunk)
            kern->destroy(large_chunk);
    }

    drivers::graphics_driver *fbs_server::get_graphics_driver() {
        return sys->get_graphics_driver();
    }

    fbscli::fbscli(service::typical_server *serv, const std::uint32_t ss_id, epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }
}
