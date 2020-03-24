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

#include <epoc/services/framework.h>

namespace eka2l1 {
    class mmf_dev_server_session: public service::typical_session {
    public:
        explicit mmf_dev_server_session(service::typical_server *serv, service::uid client_ss_uid, epoc::version client_version);
        void fetch(service::ipc_context *ctx) override;
    };

    class mmf_dev_server: public service::typical_server {
    public:
        explicit mmf_dev_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
    };
}