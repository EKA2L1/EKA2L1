/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <services/centralrepo/repo.h>
#include <kernel/server.h>

#include <atomic>
#include <map>
#include <string>
#include <unordered_map>

#define CENTRAL_REPO_UID_STRING "10202be9"
#define CENTRAL_REPO_SERVER_NAME "!CentralRepository"

namespace eka2l1 {
    class io_system;
    class device_manager;

    /**
     * \brief Parse a new centrep ini file.
	 * \returns False if IO error or invalid centrep configs.
	*/
    bool parse_new_centrep_ini(const std::string &path, central_repo &repo);

    class central_repo_server;

    struct central_repo_client_session {
        central_repo_server *server;

        std::uint32_t idcounter{ 0 };
        std::unordered_map<std::uint32_t, central_repo_client_subsession> client_subsessions;

        central_repo_client_session() {}

        void handle_message(service::ipc_context *ctx);

        void init(service::ipc_context *ctx);
        void close(service::ipc_context *ctx);

        int closerep(io_system *io, device_manager *mngr, const std::uint32_t repo_id, const std::uint32_t id);
        int closerep(io_system *io, device_manager *mngr, const std::uint32_t repo_id, decltype(client_subsessions)::iterator clisub);
    };

    // TODO for this server:
    // - Policy checks. Always fail, always success.
    // - Write UID, read UID exclusive check.
    class central_repo_server : public service::server {
        friend struct central_repo_client_session;

        // Cached repos. The key is the owner of the repo.
        std::unordered_map<std::uint32_t, std::unique_ptr<central_repo>> repos;
        std::unordered_map<std::uint64_t, central_repo_client_session> client_sessions;

        central_repos_cacher backup_cacher;
        drive_number rom_drv;

        std::atomic<std::uint32_t> id_counter;

        // These drives must be internal, aka not removeable
        std::vector<drive_number> avail_drives;
        std::mutex serv_lock;

        bool first_repo = true;

    protected:
        void rescan_drives(eka2l1::io_system *io);

        int load_repo_adv(eka2l1::io_system *io, device_manager *mngr, central_repo *repo, const std::uint32_t key,
            bool scan_org_only = false);

        eka2l1::central_repo *load_repo(eka2l1::io_system *io, device_manager *mngr, const std::uint32_t key);
        void callback_on_drive_change(eka2l1::io_system *io, const drive_number drv, int act);

        int closerep(io_system *io, const std::uint32_t repo_id, const std::uint32_t ss_id);

    public:
        void redirect_msg_to_session(service::ipc_context &ctx);

        explicit central_repo_server(eka2l1::system *sys);
        eka2l1::central_repo *get_initial_repo(eka2l1::io_system *io, device_manager *mngr, const std::uint32_t key);

        /**
         * \brief Load a repo, looking up from the cache first.
         * 
         * The function lookups in already-loaded repo list. If it's not available, load new one.
         */
        eka2l1::central_repo *load_repo_with_lookup(eka2l1::io_system *io, device_manager *mngr, const std::uint32_t key);

        void connect(service::ipc_context &ctx) override;
        void disconnect(service::ipc_context &ctx) override;
    };
}
