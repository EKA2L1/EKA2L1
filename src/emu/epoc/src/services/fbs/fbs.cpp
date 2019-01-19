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

#include <epoc/services/fbs/fbs.h>
#include <epoc/epoc.h>
#include <epoc/kernel.h>

#include <common/log.h>
#include <common/e32inc.h>

#include <e32err.h>

namespace eka2l1 {
    fbs_server::fbs_server(eka2l1::system *sys) 
        : service::server(sys, "!Fontbitmapserver", true) {
        REGISTER_IPC(fbs_server, init, fbs_init, "Fbs::Init");
    }

    void fbs_server::init(service::ipc_context context) {
        if (!shared_chunk && !large_chunk) {
            // Initialize those chunks
            kernel_system *kern = context.sys->get_kernel_system();
            std::uint32_t shared_chunk_handle = kern->create_chunk(
                "FbsSharedChunk",
                0,
                0x10000,
                0x200000,
                prot::read_write,
                kernel::chunk_type::disconnected,
                kernel::chunk_access::global,
                kernel::chunk_attrib::none,
                kernel::owner_type::kernel
            );

            std::uint32_t large_chunk_handle = kern->create_chunk(
                "FbsLargeChunk",
                0,
                0,
                0x2000000,
                prot::read_write,
                kernel::chunk_type::disconnected,
                kernel::chunk_access::global,
                kernel::chunk_attrib::none,
                kernel::owner_type::kernel
            );

            if (shared_chunk_handle == INVALID_HANDLE || large_chunk_handle == INVALID_HANDLE) {
                LOG_CRITICAL("Can't create shared chunk and large chunk of FBS, exiting");
                context.set_request_status(KErrNoMemory);

                return;
            }

            shared_chunk = std::reinterpret_pointer_cast<kernel::chunk>(kern->get_kernel_obj(shared_chunk_handle));
            large_chunk = std::reinterpret_pointer_cast<kernel::chunk>(kern->get_kernel_obj(large_chunk_handle));
        }

        // Create new server client
        fbscli cli;
        const std::uint32_t ss_id = context.msg->msg_session->unique_id();

        clients.emplace(ss_id, std::move(cli));
        context.set_request_status(ss_id);
    }
}