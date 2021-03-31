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

#include <system/epoc.h>
#include <services/sisregistry/sisregistry.h>

#include <utils/consts.h>
#include <utils/err.h>

#include <common/chunkyseri.h>
#include <common/time.h>

namespace eka2l1 {

    sisregistry_server::sisregistry_server(eka2l1::system *sys)
        : service::typical_server(sys, "!SisRegistryServer") {
    }

    void sisregistry_server::connect(service::ipc_context &context) {
        create_session<sisregistry_client_session>(&context);
        context.complete(epoc::error_none);
    }

    sisregistry_client_session::sisregistry_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void sisregistry_client_session::fetch(service::ipc_context *ctx) {
        if (ctx->sys->get_symbian_version_use() < epocver::epoc95) {
            if ((ctx->msg->function >= sisregistry_get_matching_supported_languages) &&
                (ctx->msg->function <= sisregistry_separator_minimum_read_user_data)) {
                ctx->msg->function++;
            }
        }

        switch (ctx->msg->function) {
        case sisregistry_open_registry_uid: {
            auto uuu = ctx->get_argument_data_from_descriptor<epoc::uid>(0);
            LOG_TRACE(SERVICE_SISREGISTRY, "Open new subsession for UID 0x{:X}", uuu.value());

            if (uuu.value() == 0x20003b78)
                ctx->complete(epoc::error_none);
            else
                ctx->complete(epoc::error_not_found);
            break;
        }
    
        case sisregistry_in_rom: {
            is_in_rom(ctx);
            break;
        }

        case sisregistry_trust_timestamp: {
            get_trust_timestamp(ctx);
            break;
        }

        case sisregistry_trust_status_op: {
            get_trust_status(ctx);
            break;
        }

        case sisregistry_installed_packages: {
            request_package_augmentations(ctx);
            break;
        }

        case sisregistry_installed_uid: {
            is_installed_uid(ctx);
            break;
        }

        case sisregistry_files: {
            request_files(ctx);
            break;
        }

        case sisregistry_file_descriptions: {
            request_file_descriptions(ctx);
            break;
        }
        
        case sisregistry_package_augmentations: {
            request_package_augmentations(ctx);
            break;
        }

        case sisregistry_package_op: {
            get_package(ctx);
            break;
        }
        
        case sisregistry_non_removable: {
            is_non_removable(ctx);
            break;
        }

        case sisregistry_dependent_packages: {
            request_package_augmentations(ctx);
            break;
        }

        case sisregistry_embedded_packages: {
            request_package_augmentations(ctx);
            break;
        }

        case sisregistry_signed_by_sucert: {
            is_signed_by_sucert(ctx);
            break;
        }

        case sisregistry_add_entry: {
            std::uint8_t *item_def_ptr = ctx->get_descriptor_argument_ptr(0);
            std::uint32_t size = ctx->get_argument_data_size(0);
            common::chunkyseri seri(item_def_ptr, size, common::SERI_MODE_READ);
            sisregistry_package package;
            seri.absorb(package.uid);
            epoc::absorb_des_string(package.package_name, seri, true);
            epoc::absorb_des_string(package.vendor_name, seri, true);
            seri.absorb(package.index);

            ctx->complete(epoc::error_none);
            break;
        }

        default:
            LOG_ERROR(SERVICE_SISREGISTRY, "Unimplemented opcode for sisregistry server 0x{:X}", ctx->msg->function);
            ctx->complete(epoc::error_none);
            break;
        }
    }

    void sisregistry_client_session::is_in_rom(eka2l1::service::ipc_context *ctx) {
        uint32_t in_rom = false;

        ctx->write_data_to_descriptor_argument(0, in_rom);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::get_trust_timestamp(eka2l1::service::ipc_context *ctx) {
        std::uint64_t timestamp = common::get_current_time_in_microseconds_since_1ad();

        ctx->write_data_to_descriptor_argument(0, timestamp);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::get_trust_status(eka2l1::service::ipc_context *ctx) {
        sisregistry_trust_status status;
        status.revocation_status = sisregistry_revocation_status::sisregistry_revocation_ocsp_good;
        status.validation_status = sisregistry_validation_status::sisregistry_validation_validated;

        ctx->write_data_to_descriptor_argument(0, status);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::request_files(eka2l1::service::ipc_context *ctx) {
        std::optional<sisregistry_stub_extraction_mode> package_mode = ctx->get_argument_data_from_descriptor<sisregistry_stub_extraction_mode>(0);
        LOG_TRACE(SERVICE_SISREGISTRY, "sisregistry_files 0x{:X}", package_mode.value());

        if (package_mode == sisregistry_stub_extraction_mode::sisregistry_stub_extraction_mode_get_count) {
            std::uint32_t file_count = 0;
            ctx->write_data_to_descriptor_argument<std::uint32_t>(1, file_count);
            ctx->complete(epoc::error_none);
        } else if (package_mode == sisregistry_stub_extraction_mode::sisregistry_stub_extraction_mode_get_files) {
            request_file_descriptions(ctx);
        }
    }

    void sisregistry_client_session::request_file_descriptions(eka2l1::service::ipc_context *ctx) {
        common::chunkyseri seri(nullptr, 0, common::chunkyseri_mode::SERI_MODE_MEASURE);
        populate_file_descriptions(seri);

        std::vector<char> buf(seri.size());
        seri = common::chunkyseri(reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size(),
            common::SERI_MODE_WRITE);
        populate_file_descriptions(seri);

        ctx->write_data_to_descriptor_argument(0, reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size());
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::populate_file_descriptions(common::chunkyseri &seri) {
        std::uint32_t file_count = 0;
        seri.absorb(file_count);
        for (size_t i = 0; i < file_count; i++) {
            sisregistry_file_description desc;

            epoc::absorb_des_string(desc.target, seri, true);
            epoc::absorb_des_string(desc.mime_type, seri, true);
            seri.absorb(desc.hash);
            seri.absorb(desc.operation);
            seri.absorb(desc.operation_options);
            seri.absorb(desc.uncompressed_length);
            seri.absorb(desc.index);
            seri.absorb(desc.sid);
            epoc::absorb_des_string(desc.capabilities_data, seri, true);
        }
    }

    void sisregistry_client_session::request_package_augmentations(eka2l1::service::ipc_context *ctx) {
        common::chunkyseri seri(nullptr, 0, common::chunkyseri_mode::SERI_MODE_MEASURE);
        populate_augmentations(seri);

        std::vector<char> buf(seri.size());
        seri = common::chunkyseri(reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size(),
            common::SERI_MODE_WRITE);
        populate_augmentations(seri);

        ctx->write_data_to_descriptor_argument(0, reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size());
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::populate_augmentations(common::chunkyseri &seri) {
        std::uint32_t property_count = 1;
        seri.absorb(property_count);
        for (size_t i = 0; i < property_count; i++) {
            sisregistry_package package;
            package.index = 0;

            seri.absorb(package.uid);
            epoc::absorb_des_string(package.package_name, seri, true);
            epoc::absorb_des_string(package.vendor_name, seri, true);
            seri.absorb(package.index);
        }
    }

    void sisregistry_client_session::get_package(eka2l1::service::ipc_context *ctx) {
        std::uint32_t session_id = *(ctx->get_argument_value<std::uint32_t>(3));

        sisregistry_package package;
        package.uid = session_id;
        package.index = 0;

        ctx->write_data_to_descriptor_argument(0, package);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::is_non_removable(eka2l1::service::ipc_context *ctx) {
        uint32_t removable = true;

        ctx->write_data_to_descriptor_argument(0, removable);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::is_signed_by_sucert(eka2l1::service::ipc_context *ctx) {
        uint32_t signed_by_sucert = false;

        ctx->write_data_to_descriptor_argument(0, signed_by_sucert);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::is_installed_uid(eka2l1::service::ipc_context *ctx) {
        std::optional<epoc::uid> package_uid = ctx->get_argument_data_from_descriptor<epoc::uid>(0);
        if (!package_uid) {
            ctx->complete(epoc::error_argument);
            return;
        }

        LOG_TRACE(SERVICE_SISREGISTRY, "IsInstalledUid for 0x{:X} stubbed with false", package_uid.value());
        std::int32_t result = 0;

        ctx->write_data_to_descriptor_argument<std::int32_t>(1, result);
        ctx->complete(epoc::error_none);
    }
}
