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

#include <services/etel/common.h>
#include <services/etel/etel.h>
#include <services/etel/modmngr.h>
#include <services/etel/subsess.h>
#include <services/framework.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace eka2l1 {
    using etel_subsession_instance = std::unique_ptr<etel_subsession>;
    
    std::string get_etel_server_name_by_epocver(const epocver ver);

    struct etel_session : public service::typical_session {
        epoc::etel::module_manager mngr_;
        std::vector<etel_subsession_instance> subsessions_;

    protected:
        void add_new_subsession(service::ipc_context *ctx, etel_subsession_instance &instance);

    public:
        void close_phone_module(service::ipc_context *ctx);
        void load_phone_module(service::ipc_context *ctx);
        void enumerate_phones(service::ipc_context *ctx);
        void get_phone_info_by_index(service::ipc_context *ctx);
        void get_tsy_name(service::ipc_context *ctx);
        void query_tsy_functionality(service::ipc_context *ctx);
        void line_enumerate_call(service::ipc_context *ctx);

        void open_from_session(service::ipc_context *ctx);
        void open_from_subsession(service::ipc_context *ctx);
        void close_sub(service::ipc_context *ctx);

        explicit etel_session(service::typical_server *serv, kernel::uid client_ss_uid, epoc::version client_ver);
        void fetch(service::ipc_context *ctx) override;
    };

    class etel_server : public service::typical_server {
    public:
        explicit etel_server(eka2l1::system *sys);
        void connect(service::ipc_context &ctx) override;

        bool is_oldarch();
    };
}