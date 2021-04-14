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
    void sisregistry_package::do_state(common::chunkyseri &seri) {
        seri.absorb(uid);
        epoc::absorb_des_string(package_name, seri, true);
        epoc::absorb_des_string(vendor_name, seri, true);
        seri.absorb(index);
    }

    void sisregistry_file_description::do_state(common::chunkyseri &seri) {
        epoc::absorb_des_string(target, seri, true);
        epoc::absorb_des_string(mime_type, seri, true);

        hash.do_state(seri);

        seri.absorb(operation);
        seri.absorb(operation_options);
        seri.absorb(uncompressed_length);
        seri.absorb(index);
        seri.absorb(sid);
        epoc::absorb_des_string(capabilities_data, seri, true);
    }

    void sisregistry_hash_container::do_state(common::chunkyseri &seri) {
        seri.absorb(algorithm);
        epoc::absorb_des_string(data, seri, true);
    }

    void sisregistry_trust_status::do_state(common::chunkyseri &seri) {
        seri.absorb(validation_status);
        seri.absorb(revocation_status);
        seri.absorb(result_date);
        seri.absorb(last_check_date);
        seri.absorb(quarantined);
        seri.absorb(quarantined_date);
    }

    void sisregistry_dependency::do_state(common::chunkyseri &seri) {
        seri.absorb(uid);
        seri.absorb(from_version.u32);
        seri.absorb(to_version.u32);
    }

    void sisregistry_property::do_state(common::chunkyseri &seri) {
        seri.absorb(key);
        seri.absorb(value);
    }

    void sisregistry_controller_info::do_state(common::chunkyseri &seri) {
        seri.absorb(version.u32);
        seri.absorb(offset);
        hash.do_state(seri);
    }

    void sisregistry_token::do_state(common::chunkyseri &seri) {
        sisregistry_package::do_state(seri);
        std::uint32_t count = 0;
        seri.absorb(count);
        for (uint32_t i = 0; i < count; i++) {
            sisregistry_file_description desc;
            desc.do_state(seri);
        }

        seri.absorb(drives);
        seri.absorb(completely_present);
        seri.absorb(present_removable_drives);
        seri.absorb(current_drives);
        
        seri.absorb(count);
        for (uint32_t i = 0; i < count; i++) {
            sisregistry_controller_info info;
            info.do_state(seri);
        }

        seri.absorb(version.u32);
        seri.absorb(language);
        seri.absorb(selected_drive);
        seri.absorb(unused1);
        seri.absorb(unused2);
    }

    void sisregistry_object::do_state(common::chunkyseri &seri) {
        sisregistry_token::do_state(seri);
        std::uint32_t count = 0;

        epoc::absorb_des_string(vendor_localized_name, seri, true);
        seri.absorb(install_type);

        seri.absorb(count);
        for (size_t i = 0; i < count; i++) {
            sisregistry_dependency dependency;
            dependency.do_state(seri);
        }
        seri.absorb(count);
        for (size_t i = 0; i < count; i++) {
            sisregistry_package package;
            package.do_state(seri);
        }
        seri.absorb(count);
        for (size_t i = 0; i < count; i++) {
            sisregistry_property property;
            property.do_state(seri);
        }

        seri.absorb(owned_file_descriptions);

        seri.absorb(count);
        for (size_t i = 0; i < count; i++) {
            sisregistry_file_description desc;
            desc.do_state(seri);
        }

        seri.absorb(in_rom);
        seri.absorb(signed_);
        seri.absorb(signed_by_sucert);
        seri.absorb(deletable_preinstalled);
        seri.absorb(file_major_version);
        seri.absorb(file_minor_version);
        seri.absorb(trust);
        seri.absorb(remove_with_last_dependent);
        seri.absorb(trust_timestamp);
        trust_status.do_state(seri);
        seri.absorb(count);
        for (size_t i = 0; i < count; i++) {
            std::int32_t install_chain_index = 0;
            seri.absorb(install_chain_index);
        }
        seri.absorb(count);
        for (size_t i = 0; i < count; i++) {
            std::int32_t supported_language_id = 0;
            seri.absorb(supported_language_id);
        }
        seri.absorb(count);
        for (size_t i = 0; i < count; i++) {
            std::u16string localized_package_name;
            epoc::absorb_des_string(localized_package_name, seri, true);
        }
        seri.absorb(count);
        for (size_t i = 0; i < count; i++) {
            std::u16string localized_vendor_name;
            epoc::absorb_des_string(localized_vendor_name, seri, true);
        }

        seri.absorb(is_removable);
    }

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
            open_registry_uid(ctx);
            break;
        }

        case sisregistry_version: {
            get_version(ctx);
            break;
        }
    
        case sisregistry_in_rom: {
            is_in_rom(ctx);
            break;
        }

        case sisregistry_selected_drive: {
            get_selected_drive(ctx);
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

        case sisregistry_sid_to_filename: {
            request_sid_to_filename(ctx);
            break;
        }

        case sisregistry_package_exists_in_rom: {
            is_in_rom(ctx);
            break;
        }

        case sisregistry_stub_file_entries: {
            request_stub_file_entries(ctx);
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

        case sisregistry_uid: {
            request_uid(ctx);
            break;
        }

         case sisregistry_get_entry: {
            get_entry(ctx);
            break;
        }

        case sisregistry_sids: {
            request_sids(ctx);
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

        case sisregistry_preinstalled: {
            is_preinstalled(ctx);
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

        case sisregistry_add_entry: {
            add_entry(ctx);
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

        case sisregistry_sid_to_package: {
            sid_to_package(ctx);
            break;
        }

        default:
            LOG_ERROR(SERVICE_SISREGISTRY, "Unimplemented opcode for sisregistry server 0x{:X}", ctx->msg->function);
            ctx->complete(epoc::error_none);
            break;
        }
    }

    void sisregistry_client_session::open_registry_uid(eka2l1::service::ipc_context *ctx) {
        auto uid = ctx->get_argument_data_from_descriptor<epoc::uid>(0);
        LOG_TRACE(SERVICE_SISREGISTRY, "Open new subsession for UID 0x{:X}", uid.value());

        if (uid.value() == 0x20003b78 || (uid.value() == 0x2000AFDE && server<sisregistry_server>()->added))
            ctx->complete(epoc::error_none);
        else
            ctx->complete(epoc::error_not_found);
    }

    void sisregistry_client_session::get_version(eka2l1::service::ipc_context *ctx) {
        epoc::version version;
        version.major = 1;
        version.minor = 40;
        version.build = 1557;

        ctx->write_data_to_descriptor_argument(0, version);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::is_in_rom(eka2l1::service::ipc_context *ctx) {
        uint32_t in_rom = false;

        ctx->write_data_to_descriptor_argument(0, in_rom);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::get_selected_drive(eka2l1::service::ipc_context *ctx) {
        std::int32_t result = 'E';

        ctx->write_data_to_descriptor_argument(0, result);
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

    void sisregistry_client_session::request_sid_to_filename(eka2l1::service::ipc_context *ctx) {
        std::optional<epoc::uid> uid = ctx->get_argument_data_from_descriptor<epoc::uid>(0);

        sisregistry_file_description desc;
        desc.target = u"E:\\sys\\bin\\Sims2Pets_NGage.exe";

        std::vector<std::uint8_t> buf;
        common::chunkyseri seri(nullptr, 0, common::SERI_MODE_MEASURE);
        desc.do_state(seri);

        buf.resize(seri.size());
        seri = common::chunkyseri(buf.data(), buf.size(), common::SERI_MODE_WRITE);
        desc.do_state(seri);

        ctx->write_data_to_descriptor_argument(1, buf.data(), buf.size());
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::request_stub_file_entries(eka2l1::service::ipc_context *ctx) {
        std::optional<epoc::uid> uid = ctx->get_argument_data_from_descriptor<epoc::uid>(0);
        std::optional<sisregistry_stub_extraction_mode> package_mode = ctx->get_argument_data_from_descriptor<sisregistry_stub_extraction_mode>(1);
        LOG_TRACE(SERVICE_SISREGISTRY, "sisregistry_stub_file_entries 0x{:X}", package_mode.value());

        if (package_mode == sisregistry_stub_extraction_mode::sisregistry_stub_extraction_mode_get_count) {
            std::uint32_t file_count = 0;
            ctx->write_data_to_descriptor_argument<std::uint32_t>(2, file_count);
            ctx->complete(epoc::error_none);
        } else if (package_mode == sisregistry_stub_extraction_mode::sisregistry_stub_extraction_mode_get_files) {
            common::chunkyseri seri(nullptr, 0, common::chunkyseri_mode::SERI_MODE_MEASURE);
            populate_files(seri);

            std::vector<char> buf(seri.size());
            seri = common::chunkyseri(reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size(),
                common::SERI_MODE_WRITE);
            populate_files(seri);

            ctx->write_data_to_descriptor_argument(3, reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size());
            ctx->complete(epoc::error_none);
        }
    }

    void sisregistry_client_session::request_uid(eka2l1::service::ipc_context *ctx) {
        epoc::uid uid = 0x2000AFDE;

        ctx->write_data_to_descriptor_argument(0, uid);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::get_entry(eka2l1::service::ipc_context *ctx) {
        std::optional<epoc::uid> uid = ctx->get_argument_data_from_descriptor<epoc::uid>(0);

        sisregistry_object object;

        std::vector<std::uint8_t> buf;
        common::chunkyseri seri(nullptr, 0, common::SERI_MODE_MEASURE);
        object.do_state(seri);

        buf.resize(seri.size());

        seri = common::chunkyseri(buf.data(), buf.size(), common::SERI_MODE_WRITE);
        object.do_state(seri);

        ctx->write_data_to_descriptor_argument(1, buf.data(), buf.size());
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
            common::chunkyseri seri(nullptr, 0, common::chunkyseri_mode::SERI_MODE_MEASURE);
            populate_files(seri);

            std::vector<char> buf(seri.size());
            seri = common::chunkyseri(reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size(),
                common::SERI_MODE_WRITE);
            populate_files(seri);

            ctx->write_data_to_descriptor_argument(2, reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size());
            ctx->complete(epoc::error_none);
        }
    }

    void sisregistry_client_session::populate_files(common::chunkyseri &seri) {
        std::uint32_t file_count = 0;
        seri.absorb(file_count);
        for (size_t i = 0; i < file_count; i++) {
            std::u16string filename;
            epoc::absorb_des_string(filename, seri, true);
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
            desc.do_state(seri);
        }
    }

    void sisregistry_client_session::add_entry(eka2l1::service::ipc_context *ctx) {
        std::uint8_t *item_def_ptr = ctx->get_descriptor_argument_ptr(0);
        std::uint32_t size = ctx->get_argument_data_size(0);
        common::chunkyseri seri(item_def_ptr, size, common::SERI_MODE_READ);
        sisregistry_package package;
        package.do_state(seri);

        server<sisregistry_server>()->added = true;

        ctx->complete(epoc::error_none);
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
            package.do_state(seri);
        }
    }

    void sisregistry_client_session::is_preinstalled(eka2l1::service::ipc_context *ctx) {
        std::int32_t result = 0;

        ctx->write_data_to_descriptor_argument<std::int32_t>(0, result);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::get_package(eka2l1::service::ipc_context *ctx) {
        sisregistry_package package;
        package.uid = 0x2000AFDE;
        package.index = 0x01000000;
        package.package_name = u"The Sims 2 Pets";
        package.vendor_name = u"Electronic Arts Inc.";

        std::vector<std::uint8_t> buf;
        common::chunkyseri seri(nullptr, 0, common::SERI_MODE_MEASURE);
        package.do_state(seri);

        buf.resize(seri.size());

        seri = common::chunkyseri(buf.data(), buf.size(), common::SERI_MODE_WRITE);
        package.do_state(seri);

        ctx->write_data_to_descriptor_argument(0, buf.data(), buf.size());
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
    
    void sisregistry_client_session::sid_to_package(eka2l1::service::ipc_context *ctx) {
        std::optional<epoc::uid> package_sid = ctx->get_argument_data_from_descriptor<epoc::uid>(0);
        if (!package_sid.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        if (package_sid.value() == 0x2000AFDE) {
            sisregistry_package package;
            package.uid = 0x2000AFDE;
            package.index = 0x01000000;
            package.package_name = u"The Sims 2 Pets";
            package.vendor_name = u"Electronic Arts Inc.";

            std::vector<std::uint8_t> buf;
            common::chunkyseri seri(nullptr, 0, common::SERI_MODE_MEASURE);
            package.do_state(seri);

            buf.resize(seri.size());

            seri = common::chunkyseri(buf.data(), buf.size(), common::SERI_MODE_WRITE);
            package.do_state(seri);

            ctx->write_data_to_descriptor_argument(1, buf.data(), buf.size());
            ctx->complete(epoc::error_none);

            return;
        }

        LOG_TRACE(SERVICE_SISREGISTRY, "SidToPackage for 0x{:X} stubbed with not found", package_sid.value());
        ctx->complete(epoc::error_not_found);
    }

    void sisregistry_client_session::populate_sids(common::chunkyseri &seri) {
        std::uint32_t count = 1;
        seri.absorb(count);

        std::uint32_t single_uid = 0x2000AFDE;
        seri.absorb(single_uid);
    }

    void sisregistry_client_session::request_sids(eka2l1::service::ipc_context *ctx) {
        common::chunkyseri seri(nullptr, 0, common::chunkyseri_mode::SERI_MODE_MEASURE);
        populate_sids(seri);

        std::vector<char> buf(seri.size());
        seri = common::chunkyseri(reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size(),
            common::SERI_MODE_WRITE);
        populate_sids(seri);

        ctx->write_data_to_descriptor_argument(0, reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size());
        ctx->complete(epoc::error_none);
    }
}
