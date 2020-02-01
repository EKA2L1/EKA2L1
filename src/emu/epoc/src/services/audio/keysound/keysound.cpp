/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project.
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

#include <common/chunkyseri.h>

#include <epoc/services/audio/keysound/keysound.h>
#include <epoc/services/audio/keysound/ops.h>
#include <epoc/utils/err.h>

#include <epoc/kernel/process.h>

namespace eka2l1 {
    // sf_mw_classicui document
    // Reference from GUID-1A9B515C-C20F-4EC7-B62A-223B219BBC4E, Belle devlib
    // Implementation based of original S^3 open source code and documentation.
    keysound_session::keysound_session(service::typical_server *svr, service::uid client_ss_uid, epoc::version client_version)
        : service::typical_session(svr, client_ss_uid, client_version) {
    }

    void keysound_session::init(service::ipc_context *ctx) {
        std::int32_t *allow_load_sound_info_from_resource = reinterpret_cast<std::int32_t*>(
            ctx->msg->own_thr->owning_process()->get_ptr_on_addr_space(*ctx->get_arg<kernel::uid>(0)));

        // You're welcome.
        *allow_load_sound_info_from_resource = true;

        // Store app UID
        app_uid_ = *ctx->get_arg<std::uint32_t>(1);
        ctx->set_request_status(epoc::error_none);
    }

    void keysound_session::push_context(service::ipc_context *ctx) {
        const auto item_count = ctx->get_arg<std::uint32_t>(0);
        const auto uid = ctx->get_arg<std::uint32_t>(2);
        const auto rsc_id = ctx->get_arg<std::int32_t>(3);

        std::uint8_t *item_def_ptr = ctx->get_arg_ptr(1);

        if (!item_count || !uid || !rsc_id || !item_def_ptr) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        static constexpr std::uint32_t SIZE_EACH_ITEM_DEF = 5;

        // Push the context
        common::chunkyseri seri(item_def_ptr, item_count.value() * SIZE_EACH_ITEM_DEF,
            common::SERI_MODE_READ);

        contexts_.emplace(seri, uid.value(), rsc_id.value(), item_count.value());
        ctx->set_request_status(epoc::error_none);
    }

    void keysound_session::pop_context(service::ipc_context *ctx) {

    }
    
    void keysound_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case epoc::keysound::opcode_init: {
            init(ctx);
            break;
        }

        case epoc::keysound::opcode_push_context: {
            push_context(ctx);
            break;
        }

        default:
            LOG_ERROR("Unimplemented keysound server opcode: {}", ctx->msg->function);
            break;
        }
    }

    static constexpr const char *KEYSOUND_SERVER_NAME = "KeySoundServer";

    keysound_server::keysound_server(system *sys)
        : service::typical_server(sys, KEYSOUND_SERVER_NAME) {
    }
    
    void keysound_server::connect(service::ipc_context &context) {
        create_session<keysound_session>(&context);
        context.set_request_status(0);
    }
}