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

#include <epoc/services/audio/mmf/common.h>
#include <epoc/services/audio/mmf/dev.h>
#include <epoc/utils/err.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>

#include <common/algorithm.h>
#include <common/log.h>

namespace eka2l1 {
    static const char *MMF_DEV_SERVER_NAME = "!MMFDevServer";

    mmf_dev_server::mmf_dev_server(eka2l1::system *sys)
        : service::typical_server(sys, MMF_DEV_SERVER_NAME) {
    }

    void mmf_dev_server::connect(service::ipc_context &context) {
        create_session<mmf_dev_server_session>(&context);
        context.set_request_status(epoc::error_none);
    }

    mmf_dev_server_session::mmf_dev_server_session(service::typical_server *serv, service::uid client_ss_uid, epoc::version client_version)
        : service::typical_session(serv, client_ss_uid, client_version)
        , volume_(0)
        , samples_played_(0)
        , buffer_chunk_(nullptr)
        , stream_state_(epoc::mmf_state_idle) {
        conf_ = get_caps();

        kernel_system *kern = serv->get_system()->get_kernel_system();
        buffer_chunk_ = kern->create<kernel::chunk>(kern->get_memory_system(), nullptr,
            fmt::format("MMFBufferDevChunk{}", client_ss_uid), 0, conf_.buffer_size_,
            conf_.buffer_size_, prot::read_write, kernel::chunk_type::normal,
            kernel::chunk_access::kernel_mapping, kernel::chunk_attrib::none);
    }

    void mmf_dev_server_session::max_volume(service::ipc_context *ctx) {
        std::optional<epoc::mmf_dev_sound_proxy_settings> settings
            = ctx->get_arg_packed<epoc::mmf_dev_sound_proxy_settings>(2);

        if (!settings) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        settings->max_vol_ = 100;

        ctx->write_arg_pkg<epoc::mmf_dev_sound_proxy_settings>(2, settings.value());
        ctx->set_request_status(epoc::error_none);
    }

    void mmf_dev_server_session::volume(service::ipc_context *ctx) {
        std::optional<epoc::mmf_dev_sound_proxy_settings> settings
            = ctx->get_arg_packed<epoc::mmf_dev_sound_proxy_settings>(2);

        if (!settings) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        settings->vol_ = static_cast<std::int32_t>(volume_);

        ctx->write_arg_pkg<epoc::mmf_dev_sound_proxy_settings>(2, settings.value());
        ctx->set_request_status(epoc::error_none);
    }

    void mmf_dev_server_session::set_priority_settings(service::ipc_context *ctx) {
        std::optional<epoc::mmf_priority_settings> pri_settings = ctx->get_arg_packed<epoc::mmf_priority_settings>(1);

        if (!pri_settings) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        pri_ = std::move(pri_settings.value());
        ctx->set_request_status(epoc::error_none);
    }

    void mmf_dev_server_session::set_volume(service::ipc_context *ctx) {
        std::optional<epoc::mmf_dev_sound_proxy_settings> settings
            = ctx->get_arg_packed<epoc::mmf_dev_sound_proxy_settings>(1);

        if (!settings) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        volume_ = common::clamp<std::uint32_t>(0, 100, settings->vol_);
        ctx->set_request_status(epoc::error_none);
    }

    void mmf_dev_server_session::init3(service::ipc_context *ctx) {
        ctx->set_request_status(epoc::error_none);
    }

    epoc::mmf_capabilities mmf_dev_server_session::get_caps() {
        epoc::mmf_capabilities caps;

        // Fill our preferred settings
        caps.channels_ = 2;
        caps.encoding_ = epoc::mmf_encoding_16bit_pcm;
        caps.rate_ = epoc::mmf_sample_rate_44100hz;
        caps.buffer_size_ = 8000;

        return caps;
    }

    void mmf_dev_server_session::capabilities(service::ipc_context *ctx) {
        std::optional<epoc::mmf_dev_sound_proxy_settings> settings
            = ctx->get_arg_packed<epoc::mmf_dev_sound_proxy_settings>(2);

        if (!settings) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        settings->caps_ = get_caps();

        ctx->write_arg_pkg<epoc::mmf_dev_sound_proxy_settings>(2, settings.value());
        ctx->set_request_status(epoc::error_none);
    }

    void mmf_dev_server_session::set_config(service::ipc_context *ctx) {
        std::optional<epoc::mmf_dev_sound_proxy_settings> settings
            = ctx->get_arg_packed<epoc::mmf_dev_sound_proxy_settings>(1);

        if (!settings) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        LOG_TRACE("{}", (int)settings->conf_.rate_);
        conf_ = settings->conf_;
        ctx->set_request_status(epoc::error_none);
    }

    void mmf_dev_server_session::get_config(service::ipc_context *ctx) {
        std::optional<epoc::mmf_dev_sound_proxy_settings> settings
            = ctx->get_arg_packed<epoc::mmf_dev_sound_proxy_settings>(2);

        if (!settings) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        settings->conf_ = conf_;

        ctx->write_arg_pkg<epoc::mmf_dev_sound_proxy_settings>(2, settings.value());
        ctx->set_request_status(epoc::error_none);
    }

    void mmf_dev_server_session::get_supported_input_data_types() {
        four_ccs_.clear();
        four_ccs_.push_back(PCM8_CC);
        four_ccs_.push_back(PCM16_CC);
    }

    void mmf_dev_server_session::get_supported_input_data_types(service::ipc_context *ctx) {
        get_supported_input_data_types();
        ctx->write_arg_pkg<int>(2, static_cast<int>(four_ccs_.size()));
        ctx->set_request_status(epoc::error_none);
    }

    void mmf_dev_server_session::copy_fourcc_array(service::ipc_context *ctx) {
        ctx->write_arg_pkg(2, reinterpret_cast<const std::uint8_t *>(four_ccs_.data()),
            static_cast<std::uint32_t>(four_ccs_.size() * sizeof(std::uint32_t)));

        ctx->set_request_status(epoc::error_none);
    }

    void mmf_dev_server_session::samples_played(service::ipc_context *ctx) {
        ctx->write_arg_pkg<std::uint32_t>(2, samples_played_);
        ctx->set_request_status(epoc::error_none);
    }

    void mmf_dev_server_session::stop(service::ipc_context *ctx) {
        stream_state_ = epoc::mmf_state_dead;
        ctx->set_request_status(epoc::error_none);
    }

    void mmf_dev_server_session::play_init(service::ipc_context *ctx) {
        stream_state_ = epoc::mmf_state_ready;

        // Launch the first buffer ready to be filled
        do_get_buffer_to_be_filled();
        ctx->set_request_status(epoc::error_none);
    }

    void mmf_dev_server_session::async_command(service::ipc_context *ctx) {
        auto pack = ctx->get_arg_packed<epoc::mmf_msg_destination>(0);
        std::optional<std::string> arg1 = ctx->get_arg<std::string>(1);
        std::optional<std::string> arg2 = ctx->get_arg<std::string>(2);

        LOG_TRACE("Async command stubbed");
    }

    void mmf_dev_server_session::request_resource_notification(service::ipc_context *ctx) {
        LOG_TRACE("Request resource notification stubbed");
        ctx->set_request_status(epoc::error_none);
    }

    void mmf_dev_server_session::cancel_play_error(service::ipc_context *ctx) {
        finish_info_.complete(epoc::error_cancel);
        ctx->set_request_status(epoc::error_none);
    }

    void mmf_dev_server_session::do_get_buffer_to_be_filled() {
        if (buffer_fill_info_.empty()) {
            return;
        }

        std::uint32_t buf_handle = server<mmf_dev_server>()->get_system()->get_kernel_system()->open_handle_with_thread(buffer_fill_info_.requester, buffer_chunk_, kernel::owner_type::thread);

        if (buf_handle == INVALID_HANDLE) {
            buffer_fill_info_.complete(epoc::error_general);
            return;
        }

        buffer_fill_buf_->chunk_op_ = epoc::mmf_dev_chunk_op_open;
        buffer_fill_info_.complete(buf_handle);
    }

    void mmf_dev_server_session::get_buffer_to_be_filled(service::ipc_context *ctx) {
        if (!buffer_fill_info_.empty()) {
            // A pending PlayError is here.
            ctx->set_request_status(epoc::error_bad_handle);
            return;
        }

        buffer_fill_info_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
        buffer_fill_buf_ = reinterpret_cast<epoc::mmf_dev_hw_buf *>(ctx->get_arg_ptr(2));

        if (!buffer_fill_buf_) {
            buffer_fill_info_.complete(epoc::error_argument);
        }
    }

    void mmf_dev_server_session::play_error(service::ipc_context *ctx) {
        finish_info_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
    }

    void mmf_dev_server_session::fetch(service::ipc_context *ctx) {
        if (ctx->msg->function >= epoc::mmf_dev_init4) {
            if ((ctx->msg->function < epoc::mmf_dev_buffer_to_be_filled) || (ctx->msg->function >= epoc::mmf_dev_play_dtmf_string_length)) {
                ctx->msg->function += 1;
            }
        }

        switch (ctx->msg->function) {
        case epoc::mmf_dev_init3:
            init3(ctx);
            break;

        case epoc::mmf_dev_config:
            get_config(ctx);
            break;

        case epoc::mmf_dev_set_config:
            set_config(ctx);
            break;

        case epoc::mmf_dev_capabilities:
            capabilities(ctx);
            break;

        case epoc::mmf_dev_volume:
            volume(ctx);
            break;

        case epoc::mmf_dev_max_volume:
            max_volume(ctx);
            break;

        case epoc::mmf_dev_set_volume:
            set_volume(ctx);
            break;

        case epoc::mmf_dev_buffer_to_be_filled:
            get_buffer_to_be_filled(ctx);
            break;

        case epoc::mmf_dev_play_complete_notify:
            play_error(ctx);
            break;

        case epoc::mmf_dev_play_init:
            play_init(ctx);
            break;

        case epoc::mmf_dev_cancel_play_complete_notify:
            cancel_play_error(ctx);
            break;

        case epoc::mmf_dev_stop:
            stop(ctx);
            break;

        case epoc::mmf_dev_get_supported_input_data_types:
            get_supported_input_data_types(ctx);
            break;

        case epoc::mmf_dev_copy_fourcc_array:
            copy_fourcc_array(ctx);
            break;

        case epoc::mmf_dev_samples_played:
            samples_played(ctx);
            break;

        case epoc::mmf_dev_set_priority_settings:
            set_priority_settings(ctx);
            break;

        default:
            LOG_ERROR("Unimplemented MMF dev server session opcode {}", ctx->msg->function);
            break;
        }
    }
}