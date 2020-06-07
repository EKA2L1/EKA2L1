/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <common/log.h>
#include <epoc/epoc.h>
#include <kernel/kernel.h>
#include <kernel/libmanager.h>
#include <services/audio/mmf/audio.h>
#include <services/audio/mmf/dev.h>
#include <utils/err.h>

namespace eka2l1 {
    static const char *MMF_AUDIO_SERVER_NAME = "!MMFAudioServer";

    mmf_audio_server::mmf_audio_server(eka2l1::system *sys, mmf_dev_server *dev)
        : service::typical_server(sys, MMF_AUDIO_SERVER_NAME)
        , dev_(dev)
        , flags_(0) {
    }

    void mmf_audio_server::connect(service::ipc_context &context) {
        if (!(flags_ & FLAG_INITIALIZED)) {
            init(sys->get_kernel_system());
        }

        create_session<mmf_audio_server_session>(&context);
        context.set_request_status(epoc::error_none);
    }

    void mmf_audio_server::init(kernel_system *kern) {
        // Determine if the set devsound info is available. This change our opcode list
        auto seg = kern->get_lib_manager()->load(u"mmfaudioserverproxy.dll", nullptr);

        if (seg && seg->export_count() >= 3) {
            // Originally there are only two exports: Open and GetDevSessionHandle
            // The third added later and becomes useless soon
            flags_ |= FLAG_DEVSOUND_INFO_AVAILABLE;
        }

        flags_ |= FLAG_INITIALIZED;
    }

    kernel_system *mmf_audio_server::get_kernel_system() {
        return sys->get_kernel_system();
    }

    mmf_audio_server_session::mmf_audio_server_session(service::typical_server *serv, service::uid client_ss_uid, epoc::version client_version)
        : service::typical_session(serv, client_ss_uid, client_version)
        , dev_session_(nullptr) {
        mmf_audio_server *aud_serv = server<mmf_audio_server>();

        kernel_system *kern = aud_serv->get_kernel_system();
        mmf_dev_server *dev_serv = aud_serv->get_mmf_dev_server();

        dev_session_ = kern->create<service::session>(reinterpret_cast<server_ptr>(dev_serv), 4);

        // Forcefully create
        epoc::version ver;
        ver.major = 1;
        ver.minor = 2;
        ver.build = 3;

        dev_session_->send_receive_sync(-1, eka2l1::ipc_arg{ static_cast<int>(ver.u32), 0, 0, 0 }, 0);
        dev_serv->process_accepted_msg();
    }

    mmf_audio_server_session::~mmf_audio_server_session() {
        if (dev_session_) {
            mmf_audio_server *aud_serv = server<mmf_audio_server>();
            kernel_system *kern = aud_serv->get_kernel_system();

            kern->destroy(dev_session_);
        }
    }

    void mmf_audio_server_session::set_devsound_info(service::ipc_context *ctx) {
        ctx->set_request_status(epoc::error_none);
    }

    void mmf_audio_server_session::get_dev_session(service::ipc_context *ctx) {
        mmf_audio_server *aud_serv = server<mmf_audio_server>();
        kernel_system *kern = aud_serv->get_kernel_system();

        const std::uint32_t session_handle = kern->open_handle_with_thread(ctx->msg->own_thr,
            dev_session_, kernel::owner_type::process);

        ctx->set_request_status(static_cast<int>(session_handle));
    }

    void mmf_audio_server_session::fetch(service::ipc_context *ctx) {
        if (!server<mmf_audio_server>()->is_devsound_info_available())
            ctx->msg->function += 1;

        switch (ctx->msg->function) {
        case mmf_audio_server_set_devsound_info:
            set_devsound_info(ctx);
            break;

        case mmf_audio_server_get_dev_session:
            get_dev_session(ctx);
            break;

        default:
            LOG_ERROR("Unimplemented MMF audio server session opcode {}", ctx->msg->function);
            break;
        }
    }
}