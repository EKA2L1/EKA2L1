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

#pragma once

#include <epoc/services/audio/mmf/common.h>
#include <epoc/services/framework.h>

#include <vector>

namespace eka2l1 {
    namespace kernel {
        class chunk;
    }

    constexpr std::uint32_t make_four_cc(const char c1, const char c2,
        const char c3, const char c4) {
        return (c1 | (c2 << 8) | (c3 << 16) | (c4 << 24));
    }

    constexpr std::uint32_t PCM8_CC = make_four_cc(' ', ' ', 'P', '8');
    constexpr std::uint32_t PCM16_CC = make_four_cc(' ', 'P', '1', '6');
    constexpr std::uint32_t PCMU8_CC = make_four_cc(' ', 'P', 'U', '8');
    constexpr std::uint32_t PCMU16_CC = make_four_cc('P', 'U', '1', '6');

    class mmf_dev_server_session : public service::typical_session {
        epoc::mmf_priority_settings pri_;
        std::uint32_t volume_;
        std::uint32_t samples_played_;
        std::vector<std::uint32_t> four_ccs_;

        epoc::mmf_capabilities conf_;
        std::uint32_t input_cc_;

        epoc::mmf_state stream_state_;
        kernel::chunk *buffer_chunk_;

        epoc::notify_info finish_info_;
        epoc::notify_info buffer_fill_info_;
        epoc::mmf_dev_hw_buf *buffer_fill_buf_;

        epoc::mmf_capabilities get_caps();
        void get_supported_input_data_types();

    protected:
        void do_get_buffer_to_be_filled();

    public:
        explicit mmf_dev_server_session(service::typical_server *serv, service::uid client_ss_uid, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;

        void init3(service::ipc_context *ctx);
        void set_volume(service::ipc_context *ctx);
        void volume(service::ipc_context *ctx);
        void max_volume(service::ipc_context *ctx);
        void set_priority_settings(service::ipc_context *ctx);
        void capabilities(service::ipc_context *ctx);
        void get_supported_input_data_types(service::ipc_context *ctx);
        void copy_fourcc_array(service::ipc_context *ctx);
        void set_config(service::ipc_context *ctx);
        void get_config(service::ipc_context *ctx);
        void samples_played(service::ipc_context *ctx);
        void stop(service::ipc_context *ctx);
        void play_init(service::ipc_context *ctx);
        void async_command(service::ipc_context *ctx);
        void request_resource_notification(service::ipc_context *ctx);
        void get_buffer_to_be_filled(service::ipc_context *ctx);
        void cancel_play_error(service::ipc_context *ctx);

        // Play sync, when finish complete the status
        void play_error(service::ipc_context *ctx);
    };

    class mmf_dev_server : public service::typical_server {
    public:
        explicit mmf_dev_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
    };
}