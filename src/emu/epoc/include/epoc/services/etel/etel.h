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

#include <epoc/services/etel/common.h>
#include <epoc/services/etel/etel.h>
#include <epoc/services/framework.h>

#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1 {
    enum etel_entry_type {
        etel_entry_call = 0,
        etel_entry_phone = 1
    };

    struct etel_module_entry {
        std::string tsy_name_;
        etel_entry_type type_;

        epoc::etel_phone_info phone_;
    };

    struct etel_session: public service::typical_session {
        std::vector<etel_module_entry> entries_;

    protected:
        std::optional<std::uint32_t> get_entry(const std::uint32_t org_index, const etel_entry_type type);

    public:
        void load_phone_module(service::ipc_context *ctx);
        void enumerate_phones(service::ipc_context *ctx);
        void get_phone_info_by_index(service::ipc_context *ctx);
        void get_tsy_name(service::ipc_context *ctx);

        void open_from_session(service::ipc_context *ctx);
        void open_from_subsession(service::ipc_context *ctx);
        void close_sub(service::ipc_context *ctx);

        explicit etel_session(service::typical_server *serv, service::uid client_ss_uid, epoc::version client_ver);
        void fetch(service::ipc_context *ctx) override;
    };

    class etel_server: public service::typical_server {
    public:
        explicit etel_server(eka2l1::system *sys);

        void connect(service::ipc_context &ctx) override;
    };
}