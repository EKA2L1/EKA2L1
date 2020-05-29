/*
 * Copyright (c) 2020 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
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

#include <services/framework.h>
#include <services/remcon/common.h>

#include <memory>

namespace eka2l1 {
    class remcon_server : public service::typical_server {
    public:
        explicit remcon_server(eka2l1::system *sys);

        void connect(service::ipc_context &ctx) override;
    };

    class remcon_session : public service::typical_session {
        epoc::remcon::player_type_information information_;
        epoc::remcon::client_type type_;
        std::string name_;

        std::unique_ptr<epoc::remcon::base_detail_session> detail_;

    public:
        explicit remcon_session(service::typical_server *svr, service::uid client_ss_uid, epoc::version client_ver);

        void fetch(service::ipc_context *ctx) override;
        void set_player_type(service::ipc_context *ctx);
    };
}