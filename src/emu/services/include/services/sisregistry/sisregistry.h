/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <common/container.h>
#include <common/uid.h>
#include <utils/des.h>

#include <kernel/server.h>

#include <services/framework.h>
#include <services/sisregistry/common.h>

#include <memory>

namespace eka2l1 {
    namespace common {
        class chunkyseri;
    }

    namespace manager {
        class packages;
    }

    namespace package {
        struct object;
    }

    class sisregistry_server : public service::typical_server {
    public:
        explicit sisregistry_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
    };

    struct sisregistry_client_subsession {
    protected:
        epoc::uid package_uid_;
        std::int32_t index_ = 0;

        manager::packages *package_manager(service::ipc_context *ctx);
        package::object *package_object(service::ipc_context *ctx);

        void populate_sids(common::chunkyseri &seri);

    public:
        explicit sisregistry_client_subsession(const epoc::uid package_uid, const std::int32_t index = 0);

        bool fetch(service::ipc_context *ctx);
        
        void get_version(eka2l1::service::ipc_context *ctx);
        void is_in_rom(eka2l1::service::ipc_context *ctx);
        void get_selected_drive(eka2l1::service::ipc_context *ctx);
        void request_files(eka2l1::service::ipc_context *ctx);
        void request_uid(eka2l1::service::ipc_context *ctx);
        void get_entry(eka2l1::service::ipc_context *ctx);
        void request_stub_file_entries(eka2l1::service::ipc_context *ctx);
        void request_file_descriptions(eka2l1::service::ipc_context *ctx);
        void populate_file_descriptions(common::chunkyseri &seri);
        void request_package_augmentations(eka2l1::service::ipc_context *ctx);
        void is_non_removable(eka2l1::service::ipc_context *ctx);
        void add_entry(eka2l1::service::ipc_context *ctx);
        void is_preinstalled(eka2l1::service::ipc_context *ctx);
        void get_package(eka2l1::service::ipc_context *ctx);
        void get_trust_timestamp(eka2l1::service::ipc_context *ctx);
        void get_trust_status(eka2l1::service::ipc_context *ctx);
        void request_sid_to_filename(eka2l1::service::ipc_context *ctx);
        void is_signed_by_sucert(eka2l1::service::ipc_context *ctx);
        void sid_to_package(eka2l1::service::ipc_context *ctx);
        void request_sids(eka2l1::service::ipc_context *ctx);
        void close_registry(eka2l1::service::ipc_context *ctx);
    };

    using sisregistry_client_subsession_inst = std::unique_ptr<sisregistry_client_subsession>;

    struct sisregistry_client_session : public service::typical_session {
    private:
        common::identity_container<sisregistry_client_subsession_inst> subsessions_;
    public:
        explicit sisregistry_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void open_registry_uid(eka2l1::service::ipc_context *ctx);
        void open_registry_by_package(eka2l1::service::ipc_context *ctx);
        void installed_uids(eka2l1::service::ipc_context *ctx);
        void installed_packages(eka2l1::service::ipc_context *ctx);
        void is_installed_uid(eka2l1::service::ipc_context *ctx);
    };
}
