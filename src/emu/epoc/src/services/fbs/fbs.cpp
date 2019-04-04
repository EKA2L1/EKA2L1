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
#include <epoc/kernel.h>
#include <epoc/kernel/chunk.h>
#include <epoc/services/fbs/fbs.h>

#include <common/cvt.h>
#include <common/e32inc.h>
#include <common/log.h>
#include <common/vecx.h>

#include <epoc/vfs.h>

#include <e32err.h>

namespace eka2l1 {
    fbs_chunk_allocator::fbs_chunk_allocator(chunk_ptr de_chunk, std::uint8_t *dat_ptr)
        : block_allocator(dat_ptr, de_chunk->get_size())
        , target_chunk(std::move(de_chunk)) {
    }

    bool fbs_chunk_allocator::expand(std::size_t target) {
        return target_chunk->adjust(target);
    }

    void fbscli::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case fbs_init: {
            connection_id_ = server<fbs_server>()->init();
            ctx->set_request_status(client_ss_uid_);
            
            break;
        }

        case fbs_nearest_font_design_height_in_pixels: {
            get_nearest_font(ctx);
            break;
        }

        case fbs_bitmap_load: {
            load_bitmap(ctx);
            break;
        }

        default: {
            LOG_ERROR("Unhandled FBScli opcode 0x{:X}", ctx->msg->function);
            break;
        }
        }
    }

    fbs_server::fbs_server(eka2l1::system *sys)
        : service::typical_server(sys, "!Fontbitmapserver") {
    }

    void fbs_server::connect(service::ipc_context &context) {
        if (!shared_chunk && !large_chunk) {
            // Initialize those chunks
            kernel_system *kern = context.sys->get_kernel_system();
            shared_chunk = kern->create<kernel::chunk>(
                kern->get_memory_system(),
                kern->crr_process(),
                "FbsSharedChunk",
                0,
                0x10000,
                0x200000,
                prot::read_write,
                kernel::chunk_type::normal,
                kernel::chunk_access::global,
                kernel::chunk_attrib::none);

            large_chunk = kern->create<kernel::chunk>(
                kern->get_memory_system(),
                kern->crr_process(),
                "FbsLargeChunk",
                0,
                0,
                0x2000000,
                prot::read_write,
                kernel::chunk_type::normal,
                kernel::chunk_access::global,
                kernel::chunk_attrib::none);

            if (!shared_chunk || !large_chunk) {
                LOG_CRITICAL("Can't create shared chunk and large chunk of FBS, exiting");
                context.set_request_status(KErrNoMemory);

                return;
            }

            memory_system *mem = sys->get_memory_system();

            base_shared_chunk = shared_chunk->base().get(mem);
            base_large_chunk = large_chunk->base().get(mem);

            shared_chunk_allocator = std::make_unique<fbs_chunk_allocator>(shared_chunk,
                base_shared_chunk);

            large_chunk_allocator = std::make_unique<fbs_chunk_allocator>(large_chunk,
                base_large_chunk);

            // Probably also indicates that font aren't loaded yet
            load_fonts(context.sys->get_io_system());

            fs_server = kern->get_by_name<service::server>("!FileServer");
        }

        // Create new server client
        create_session<fbscli>(&context);
        context.set_request_status(KErrNone);
    }

    service::uid fbs_server::init() {
        return connection_id_counter++;
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
}
