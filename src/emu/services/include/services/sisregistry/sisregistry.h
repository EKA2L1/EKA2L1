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

#include <common/uid.h>
#include <utils/des.h>

#include <kernel/server.h>
#include <services/framework.h>

namespace eka2l1 {
    enum sisregistry_opcode {
        sisregistry_open_registry_uid = 0x0,
        sisregistry_close_registry_entry = 0x3,
        sisregistry_version = 0x5,
        sisregistry_in_rom = 0xA,
        sisregistry_trust_timestamp = 0x16,
        sisregistry_package_augmentations = 0x30,
        sisregistry_package = 0x34,
        sisregistry_non_removable = 0x36,
        sisregistry_update_entry = 0x45,
        sisregistry_embedded_packages = 0x104,
        sisregistry_signed_by_sucert = 0x107
    };

    struct sisregistry_package_ {
        epoc::uid uid;
        std::u16string package_name;
        std::u16string vendor_name;
        std::int32_t index; 
    };

    class sisregistry_server : public service::typical_server {

    public:
        explicit sisregistry_server(eka2l1::system *sys);

        void connect(service::ipc_context &context) override;
    };

    struct sisregistry_client_session : public service::typical_session {
    public:
        explicit sisregistry_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void is_in_rom(eka2l1::service::ipc_context *ctx);
        void request_package_augmentations(eka2l1::service::ipc_context *ctx);
        void populate_augmentations(common::chunkyseri &seri);
        void is_non_removable(eka2l1::service::ipc_context *ctx);
        void get_package(eka2l1::service::ipc_context *ctx);
        void get_trust_timestamp(eka2l1::service::ipc_context *ctx);
        void is_signed_by_sucert(eka2l1::service::ipc_context *ctx);
    };
}
