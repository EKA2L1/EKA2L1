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

#pragma once

#include <services/audio/keysound/context.h>
#include <services/framework.h>

#include <drivers/audio/stream.h>

#include <memory>
#include <stack>
#include <string>
#include <vector>

namespace eka2l1 {
    class keysound_session : public service::typical_session {
    private:
        service::uid app_uid_; ///< The UID3 of the app opening this session
        std::vector<epoc::keysound::context> contexts_; ///< Context stack describes sound to play when key action trigger.

        std::unique_ptr<drivers::audio_output_stream> aud_out_;

        struct parser_state {
            std::uint32_t frames_;
            std::uint32_t target_freq_;

            std::uint32_t frequency_;
            std::uint32_t ms_;
            std::uint32_t duration_unit_;

            std::size_t parser_pos_;
            epoc::keysound::sound_info sound_;

            explicit parser_state();
        } state_;

    public:
        explicit keysound_session(service::typical_server *svr, service::uid client_ss_uid, epoc::version client_version);
        ~keysound_session() override {}

        std::size_t play_sounds(std::int16_t *buffer, std::size_t frames);

        void fetch(service::ipc_context *ctx) override;

        void init(service::ipc_context *ctx);
        void push_context(service::ipc_context *ctx);
        void pop_context(service::ipc_context *ctx);
        void play_sid(service::ipc_context *ctx);
        void add_sids(service::ipc_context *ctx);
        void bring_to_foreground(service::ipc_context *ctx);
        void lock_context(service::ipc_context *ctx);
    };

    class keysound_server : public service::typical_server {
        bool inited_;
        std::vector<epoc::keysound::sound_info> sounds_;

    public:
        explicit keysound_server(system *sys);
        void connect(service::ipc_context &context) override;

        void add_sound(epoc::keysound::sound_info &info) {
            sounds_.push_back(std::move(info));
        }

        epoc::keysound::sound_info *get_sound(const std::uint32_t sid);

        bool initialized() const {
            return inited_;
        }

        void initialized(const bool is_it) {
            inited_ = is_it;
        }
    };
}