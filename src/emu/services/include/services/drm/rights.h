/*
 * Copyright (c) 2018 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
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

#include <kernel/server.h>
#include <services/framework.h>

namespace eka2l1 {
    // See DRMEngineClientServer.h in omadrm/drmengine/server/inc
    // This belongs to the mw.drm repository.
    enum rights_opcode {
        rights_opcode_none,
        rights_opcode_get_key = 7,
        rights_opcode_initialize_key = 22,
        rights_opcode_decrypt = 24
    };

    static constexpr std::uint32_t ERROR_CA_NO_RIGHTS = -17452;
    static constexpr std::uint32_t ERROR_CA_PENDING_RIGHTS = -17455;

    class rights_server : public service::typical_server {
    public:
        explicit rights_server(eka2l1::system *sys);

        void connect(service::ipc_context &context) override;
    };

    struct rights_client_session : public service::typical_session {
        explicit rights_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
    };
}