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

#include <services/drm/rights/db.h>
#include <kernel/server.h>
#include <services/framework.h>

#include <string>

namespace eka2l1 {
    // See DRMEngineClientServer.h in omadrm/drmengine/server/inc
    // This belongs to the mw.drm repository.
    enum rights_opcode {
        rights_opcode_none,
        rights_opcode_get_entry_list = 3,
        rights_opcode_get_key = 7,
        rights_opcode_consume = 11,
        rights_opcode_initialize_key = 22,
        rights_opcode_decrypt = 24,
        rights_opcode_check_consume = 27
    };

    enum rights_crediental_check_status {
        rights_crediental_not_checked,
        rights_crediental_checked_and_allowed,
        rights_crediental_checked_and_denied
    };

    static constexpr std::uint32_t ERROR_CA_NO_RIGHTS = -17452;
    static constexpr std::uint32_t ERROR_CA_PENDING_RIGHTS = -17455;
    static constexpr const char* RIGHTS_SERVER_NAME = "!RightsServer";

    class rights_server : public service::typical_server {
    private:
        std::unique_ptr<epoc::drm::rights_database> database_;

        void initialize();
        void startup_imports();

    public:
        explicit rights_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
        bool import_ng2l(const std::string &content, std::vector<std::string> &success_game_name,
                         std::vector<std::string> &failed_game_name);

        epoc::drm::rights_database &database() {
            return *database_;
        }

        std::uint32_t get_suitable_seri_version() const;
    };

    struct rights_client_session : public service::typical_session {
    private:
        std::string current_key_;
        rights_crediental_check_status check_status_;

        void verify_crediental(kernel::process *client, const std::string &cid, const epoc::drm::rights_intent intent);

    public:
        explicit rights_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void get_entry_list(service::ipc_context *ctx);
        void init_key(service::ipc_context *ctx);
        void get_key(service::ipc_context *ctx);
        void consume(service::ipc_context *ctx);
        void check_consume(service::ipc_context *ctx);
    };
}