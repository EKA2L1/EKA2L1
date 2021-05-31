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

#include <package/registry.h>
#include <package/manager.h>

#include <utils/consts.h>
#include <utils/err.h>

#include <common/chunkyseri.h>
#include <common/time.h>

namespace eka2l1 {
    static void populate_all_packages(common::chunkyseri &seri, manager::packages *mngr) {
        std::uint32_t package_count = static_cast<std::uint32_t>(mngr->package_count());
        seri.absorb(package_count);

        for (const auto &pkg: *mngr) {
            package::package copied_package = static_cast<const package::package&>(pkg.second);
            copied_package.do_state(seri);
        }
    }

    static void populate_packages(common::chunkyseri &seri, std::vector<package::object *> &pkgs) {
        std::uint32_t property_count = static_cast<std::uint32_t>(pkgs.size());
        seri.absorb(property_count);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            pkgs.resize(property_count);
        }

        for (size_t i = 0; i < property_count; i++) {
            package::package copied_package = static_cast<package::package&>(*pkgs[i]);
            copied_package.do_state(seri);

            if (seri.get_seri_mode() == common::SERI_MODE_READ) {
                static_cast<package::package&>(*pkgs[i]) = copied_package;
            }
        }
    }

    static std::size_t populate_filenames_with_limitation(common::chunkyseri &seri, package::object *obj, const std::uint32_t start_index, const std::size_t max_size, std::int32_t &fcount) {
        seri.absorb(fcount);
        bool count_filename = (fcount == -1);

        if (count_filename) {
            fcount = 0;
        }

        std::size_t last_size = 0;

        for (std::size_t i = start_index; i < (count_filename ? obj->file_descriptions.size() : (start_index + fcount)); i++) {
            epoc::absorb_des_string(obj->file_descriptions[i].target, seri, true);

            if (count_filename) {
                if (seri.size() <= max_size) {
                    last_size = seri.size();
                    fcount++;
                } else {
                    break;
                }
            }
        }
        
        // Can't possibly continue
        if (!obj->file_descriptions.empty() && (fcount == 0)) {
            return static_cast<std::size_t>(-1);
        }

        return last_size;
    }

    static void populate_uids(common::chunkyseri &seri, std::vector<manager::uid> &uids) {
        std::uint32_t uid_count = static_cast<std::uint32_t>(uids.size());
        seri.absorb(uid_count);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            uids.resize(uid_count);
        }

        for (size_t i = 0; i < uid_count; i++) {
            seri.absorb(uids[i]);
        }
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

    sisregistry_client_subsession::sisregistry_client_subsession(const epoc::uid package_uid, const std::int32_t index)
        : package_uid_(package_uid)
        , index_(index) {
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

        case sisregistry_open_registry_package: {
            open_registry_by_package(ctx);
            break;
        }

        case sisregistry_installed_uid: {
            is_installed_uid(ctx);
            break;
        }

        case sisregistry_installed_uids: {
            installed_uids(ctx);
            break;
        }

        case sisregistry_installed_packages: {
            installed_packages(ctx);
            break;
        }
        
        case sisregistry_add_entry: {
            add_entry(ctx);
            break;
        }

        default: {
            // it's not real subsession. An integer
            std::optional<std::uint32_t> handle = ctx->get_argument_value<std::uint32_t>(3);
            if (handle.has_value()) {
                sisregistry_client_subsession_inst *inst = subsessions_.get(handle.value());
                if (inst) {
                    if ((*inst)->fetch(ctx)) {
                        subsessions_.remove(handle.value());
                    }
                    break;
                }
            }

            LOG_ERROR(SERVICE_SISREGISTRY, "Unimplemented opcode for sisregistry session 0x{:X}", ctx->msg->function);
            ctx->complete(epoc::error_none);
            break;
        }
        }
    }

    bool sisregistry_client_subsession::fetch(service::ipc_context *ctx) {
        bool del = false;

        switch (ctx->msg->function) {
        case sisregistry_close_registry_entry: {
            close_registry(ctx);
            del = true;

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

        case sisregistry_uid: {
            request_uid(ctx);
            break;
        }

        case sisregistry_non_removable: {
            is_non_removable(ctx);
            break;
        }
        
        case sisregistry_package_augmentations: {
            request_package_augmentations(ctx);
            break;
        }

        case sisregistry_files: {
            request_files(ctx);
            break;
        }

        case sisregistry_package_op: {
            get_package(ctx);
            break;
        }

        /*
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

         case sisregistry_get_entry: {
            get_entry(ctx);
            break;
        }

        case sisregistry_sids: {
            request_sids(ctx);
            break;
        }

        case sisregistry_file_descriptions: {
            request_file_descriptions(ctx);
            break;
        }

        case sisregistry_preinstalled: {
            is_preinstalled(ctx);
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
        }*/

        default:
            LOG_ERROR(SERVICE_SISREGISTRY, "Unimplemented opcode for sisregistry subsession 0x{:X}", ctx->msg->function);
            ctx->complete(epoc::error_none);
            break;
        }

        return del;
    }

    void sisregistry_client_session::open_registry_uid(eka2l1::service::ipc_context *ctx) {
        auto uid = ctx->get_argument_data_from_descriptor<epoc::uid>(0);
        LOG_TRACE(SERVICE_SISREGISTRY, "Open new subsession for UID 0x{:X}", uid.value());

        // Do a check of the object inside
        manager::packages *pkgmngr = ctx->sys->get_packages();
        if (!pkgmngr) {
            ctx->complete(epoc::error_general);
            return;
        }

        const package::object *obj = pkgmngr->package(uid.value());
        if (!obj) {
            LOG_ERROR(SERVICE_SISREGISTRY, "Package with UID 0x{:X} not found!", uid.value());
            ctx->complete(epoc::error_not_found);

            return;
        }

        sisregistry_client_subsession_inst inst = std::make_unique<sisregistry_client_subsession>(uid.value());
        std::uint32_t handle = static_cast<std::uint32_t>(subsessions_.add(inst));

        ctx->write_data_to_descriptor_argument<std::uint32_t>(3, handle);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::open_registry_by_package(eka2l1::service::ipc_context *ctx) {
        manager::packages *pkgmngr = ctx->sys->get_packages();
        if (!pkgmngr) {
            ctx->complete(epoc::error_general);
            return;
        }

        std::uint8_t *buffer = ctx->get_descriptor_argument_ptr(0);
        std::size_t buffer_size = ctx->get_argument_max_data_size(0);

        if (!buffer || (buffer_size == 0)) {
            ctx->complete(epoc::error_argument);
            return;
        }

        common::chunkyseri seri(buffer, buffer_size, common::SERI_MODE_READ);

        package::package need_to_open;
        need_to_open.do_state(seri);

        package::object *real_package = pkgmngr->package(need_to_open.uid, need_to_open.index);
        if (!real_package) {
            LOG_ERROR(PACKAGE, "Fail to open package with UID=0x{:X}, index={}", need_to_open.uid, need_to_open.index);
            ctx->complete(epoc::error_not_found);

            return;
        }
    
        sisregistry_client_subsession_inst inst = std::make_unique<sisregistry_client_subsession>(real_package->uid, real_package->index);
        std::uint32_t handle = static_cast<std::uint32_t>(subsessions_.add(inst));

        ctx->write_data_to_descriptor_argument<std::uint32_t>(3, handle);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::installed_uids(eka2l1::service::ipc_context *ctx) {
        manager::packages *pkgmngr = ctx->sys->get_packages();
        if (!pkgmngr) {
            ctx->complete(epoc::error_general);
            return;
        }

        std::vector<manager::uid> uids_installed = pkgmngr->installed_uids();
        const std::size_t max_buffer_size = ctx->get_argument_max_data_size(0);

        common::chunkyseri seri(nullptr, 0, common::chunkyseri_mode::SERI_MODE_MEASURE);
        populate_uids(seri, uids_installed);

        if (seri.size() > max_buffer_size) {
            ctx->write_data_to_descriptor_argument<std::uint32_t>(0, static_cast<std::uint32_t>(seri.size()));
            ctx->complete(epoc::error_overflow);

            return;
        }

        std::vector<char> buf(seri.size());
        seri = common::chunkyseri(reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size(), common::SERI_MODE_WRITE);
        populate_uids(seri, uids_installed);

        ctx->write_data_to_descriptor_argument(0, reinterpret_cast<std::uint8_t*>(&buf[0]), static_cast<std::uint32_t>(buf.size()));
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::installed_packages(eka2l1::service::ipc_context *ctx) {
        manager::packages *pkgmngr = ctx->sys->get_packages();
        if (!pkgmngr) {
            ctx->complete(epoc::error_general);
            return;
        }

        const std::size_t max_buffer_size = ctx->get_argument_max_data_size(0);

        common::chunkyseri seri(nullptr, 0, common::chunkyseri_mode::SERI_MODE_MEASURE);
        populate_all_packages(seri, pkgmngr);

        if (seri.size() > max_buffer_size) {
            ctx->write_data_to_descriptor_argument<std::uint32_t>(0, static_cast<std::uint32_t>(seri.size()));
            ctx->complete(epoc::error_overflow);

            return;
        }

        std::vector<char> buf(seri.size());
        seri = common::chunkyseri(reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size(), common::SERI_MODE_WRITE);
        populate_all_packages(seri, pkgmngr);

        ctx->write_data_to_descriptor_argument(0, reinterpret_cast<std::uint8_t*>(&buf[0]), static_cast<std::uint32_t>(buf.size()));
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::is_installed_uid(eka2l1::service::ipc_context *ctx) {
        manager::packages *pkgmngr = ctx->sys->get_packages();
        if (!pkgmngr) {
            ctx->complete(epoc::error_general);
            return;
        }

        std::optional<epoc::uid> package_uid = ctx->get_argument_data_from_descriptor<epoc::uid>(0);
        if (!package_uid.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::int32_t package_installed = static_cast<std::int32_t>(pkgmngr->installed(package_uid.value()));

        ctx->write_data_to_descriptor_argument(1, package_installed);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::add_entry(eka2l1::service::ipc_context *ctx) {
        manager::packages *mngr = ctx->sys->get_packages();
        if (!mngr) {
            ctx->complete(epoc::error_general);
            return;
        }

        std::uint8_t *item_def_ptr = ctx->get_descriptor_argument_ptr(0);
        std::size_t item_def_size = ctx->get_argument_data_size(0);
        
        if (!item_def_ptr || !item_def_size) {
            ctx->complete(epoc::error_argument);
            return;
        }

        common::chunkyseri seri(item_def_ptr, item_def_size, common::SERI_MODE_READ);

        package::object obj;
        obj.do_state(seri);

        manager::controller_info controller_info;
        controller_info.data_ = ctx->get_descriptor_argument_ptr(2);
        controller_info.size_ = ctx->get_argument_data_size(2);

        if (!mngr->add_package(obj, &controller_info)) {
            ctx->complete(epoc::error_general);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    manager::packages *sisregistry_client_subsession::package_manager(service::ipc_context *ctx) {
        return ctx->sys->get_packages();
    }

    package::object *sisregistry_client_subsession::package_object(service::ipc_context *ctx) {
        manager::packages *pkgmngr = package_manager(ctx);
        if (!pkgmngr) {
            return nullptr;
        }

        return pkgmngr->package(package_uid_, index_);
    }

    void sisregistry_client_subsession::close_registry(eka2l1::service::ipc_context *ctx) {
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_subsession::get_version(eka2l1::service::ipc_context *ctx) {
        package::object *obj = package_object(ctx);
        if (!obj) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->write_data_to_descriptor_argument(0, obj->version.u32);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_subsession::is_in_rom(eka2l1::service::ipc_context *ctx) {
        package::object *obj = package_object(ctx);
        if (!obj) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->write_data_to_descriptor_argument(0, obj->in_rom);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_subsession::get_selected_drive(eka2l1::service::ipc_context *ctx) {
        package::object *obj = package_object(ctx);
        if (!obj) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->write_data_to_descriptor_argument(0, obj->selected_drive);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_subsession::get_trust_timestamp(eka2l1::service::ipc_context *ctx) {
        package::object *obj = package_object(ctx);
        if (!obj) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->write_data_to_descriptor_argument(0, obj->trust_timestamp);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_subsession::get_trust_status(eka2l1::service::ipc_context *ctx) {
        package::object *obj = package_object(ctx);
        if (!obj) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->write_data_to_descriptor_argument(0, obj->trust_status_value);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_subsession::request_sid_to_filename(eka2l1::service::ipc_context *ctx) {
        std::optional<epoc::uid> uid = ctx->get_argument_data_from_descriptor<epoc::uid>(0);

        package::file_description desc;
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

    void sisregistry_client_subsession::request_stub_file_entries(eka2l1::service::ipc_context *ctx) {
        /*
        std::optional<epoc::uid> uid = ctx->get_argument_data_from_descriptor<epoc::uid>(0);
        std::optional<sisregistry_stub_extraction_mode> package_mode = ctx->get_argument_data_from_descriptor<sisregistry_stub_extraction_mode>(1);
        LOG_TRACE(SERVICE_SISREGISTRY, "sisregistry_stub_file_entries 0x{:X}", package_mode.value());

        if (package_mode == sisregistry_stub_extraction_mode::sisregistry_stub_extraction_mode_get_count) {
            std::uint32_t file_count = 0;
            ctx->write_data_to_descriptor_argument<std::uint32_t>(2, file_count);
            ctx->complete(epoc::error_none);
        } else if (package_mode == sisregistry_stub_extraction_mode::sisregistry_stub_extraction_mode_get_files) {
            std::int32_t total_filename_can_store = -1;

            common::chunkyseri seri(nullptr, 0, common::chunkyseri_mode::SERI_MODE_MEASURE);
            const std::size_t final_size = populate_filenames_with_limitation()

            std::vector<char> buf(seri.size());
            seri = common::chunkyseri(reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size(),
                common::SERI_MODE_WRITE);
            populate_files(seri);

            ctx->write_data_to_descriptor_argument(3, reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size());
            ctx->complete(epoc::error_none);
        }*/
    }

    void sisregistry_client_subsession::request_uid(eka2l1::service::ipc_context *ctx) {
        package::object *obj = package_object(ctx);
        if (!obj) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->write_data_to_descriptor_argument(0, obj->uid);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_subsession::get_entry(eka2l1::service::ipc_context *ctx) {
        std::optional<epoc::uid> uid = ctx->get_argument_data_from_descriptor<epoc::uid>(0);

        package::object object;

        std::vector<std::uint8_t> buf;
        common::chunkyseri seri(nullptr, 0, common::SERI_MODE_MEASURE);
        object.do_state(seri);

        buf.resize(seri.size());

        seri = common::chunkyseri(buf.data(), buf.size(), common::SERI_MODE_WRITE);
        object.do_state(seri);

        ctx->write_data_to_descriptor_argument(1, buf.data(), buf.size());
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_subsession::request_files(eka2l1::service::ipc_context *ctx) {
        package::object *obj = package_object(ctx);
        if (!obj) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        std::optional<sisregistry_stub_extraction_mode> package_mode = ctx->get_argument_data_from_descriptor<sisregistry_stub_extraction_mode>(0);

        if (package_mode == sisregistry_stub_extraction_mode::sisregistry_stub_extraction_mode_get_count) {
            std::uint32_t file_count = static_cast<std::uint32_t>(obj->file_descriptions.size());
            ctx->write_data_to_descriptor_argument<std::uint32_t>(1, file_count);
        } else if (package_mode == sisregistry_stub_extraction_mode::sisregistry_stub_extraction_mode_get_files) {
            std::optional<std::uint32_t> start_index = ctx->get_argument_data_from_descriptor<std::uint32_t>(1);
            if (!start_index.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            std::size_t max_buffer_size = ctx->get_argument_max_data_size(2);
            std::int32_t total_filename_can_store = -1;

            if (!max_buffer_size) {
                ctx->complete(epoc::error_argument);
                return;
            }

            common::chunkyseri seri(nullptr, 0, common::chunkyseri_mode::SERI_MODE_MEASURE);
            const std::size_t final_size = populate_filenames_with_limitation(seri, obj, start_index.value(), max_buffer_size, total_filename_can_store);

            if (final_size == static_cast<std::size_t>(-1)) {
                LOG_ERROR(SERVICE_SISREGISTRY, "Buffer too small to hold at least one filename!");

                ctx->complete(epoc::error_no_memory);
                return;
            }

            std::vector<char> buf(final_size);
            seri = common::chunkyseri(reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size(), common::SERI_MODE_WRITE);
            populate_filenames_with_limitation(seri, obj, start_index.value(), final_size, total_filename_can_store);

            ctx->write_data_to_descriptor_argument(2, reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size());
        } else {
            LOG_ERROR(SERVICE_SISREGISTRY, "Unidentified stub extraction mode {}", static_cast<std::int32_t>(package_mode.value()));
        }

        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_subsession::request_file_descriptions(eka2l1::service::ipc_context *ctx) {
        common::chunkyseri seri(nullptr, 0, common::chunkyseri_mode::SERI_MODE_MEASURE);
        populate_file_descriptions(seri);

        std::vector<char> buf(seri.size());
        seri = common::chunkyseri(reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size(),
            common::SERI_MODE_WRITE);
        populate_file_descriptions(seri);

        ctx->write_data_to_descriptor_argument(0, reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size());
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_subsession::populate_file_descriptions(common::chunkyseri &seri) {
        std::uint32_t file_count = 0;
        seri.absorb(file_count);
        for (size_t i = 0; i < file_count; i++) {
            package::file_description desc;
            desc.do_state(seri);
        }
    }

    void sisregistry_client_subsession::request_package_augmentations(eka2l1::service::ipc_context *ctx) {        
        package::object *obj = package_object(ctx);
        if (!obj) {
            ctx->complete(epoc::error_not_found);
            return;
        }
        
        manager::packages *mngr = package_manager(ctx);
        const std::size_t max_buffer_size = ctx->get_argument_max_data_size(0);

        std::vector<package::object *> results;

        if (obj->install_type != package::install_type_augmentations) {
            results = mngr->augmentations(obj->uid);
        }

        common::chunkyseri seri(nullptr, 0, common::chunkyseri_mode::SERI_MODE_MEASURE);
        populate_packages(seri, results);

        if (seri.size() > max_buffer_size) {
            ctx->write_data_to_descriptor_argument<std::uint32_t>(0, static_cast<std::uint32_t>(seri.size()));
            ctx->complete(epoc::error_overflow);

            return;
        }

        std::vector<char> buf(seri.size());
        seri = common::chunkyseri(reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size(), common::SERI_MODE_WRITE);
        populate_packages(seri, results);

        ctx->write_data_to_descriptor_argument(0, reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size());
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_subsession::is_preinstalled(eka2l1::service::ipc_context *ctx) {
        std::int32_t result = 0;

        ctx->write_data_to_descriptor_argument<std::int32_t>(0, result);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_subsession::get_package(eka2l1::service::ipc_context *ctx) {
        package::object *obj = package_object(ctx);
        if (!obj) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        package::package pkg = static_cast<package::package&>(*obj);

        std::vector<std::uint8_t> buf;
        common::chunkyseri seri(nullptr, 0, common::SERI_MODE_MEASURE);
        pkg.do_state(seri);

        if (seri.size() > ctx->get_argument_max_data_size(0)) {
            std::uint32_t expected_size = static_cast<std::uint32_t>(seri.size());

            ctx->write_data_to_descriptor_argument<std::uint32_t>(0, expected_size);
            ctx->complete(epoc::error_overflow);
            return;
        }

        buf.resize(seri.size());

        seri = common::chunkyseri(buf.data(), buf.size(), common::SERI_MODE_WRITE);
        pkg.do_state(seri);

        ctx->write_data_to_descriptor_argument(0, buf.data(), static_cast<std::uint32_t>(buf.size()));
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_subsession::is_non_removable(eka2l1::service::ipc_context *ctx) {
        // It's actually removable, lol
        package::object *obj = package_object(ctx);
        if (!obj) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->write_data_to_descriptor_argument(0, obj->is_removable);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_subsession::is_signed_by_sucert(eka2l1::service::ipc_context *ctx) {
        package::object *obj = package_object(ctx);
        if (!obj) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->write_data_to_descriptor_argument(0, obj->signed_by_sucert);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_subsession::sid_to_package(eka2l1::service::ipc_context *ctx) {
        std::optional<epoc::uid> package_sid = ctx->get_argument_data_from_descriptor<epoc::uid>(0);
        if (!package_sid.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        if (package_sid.value() == 0x2000AFDE) {
            package::package package;
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

    void sisregistry_client_subsession::populate_sids(common::chunkyseri &seri) {
        std::uint32_t count = 1;
        seri.absorb(count);

        std::uint32_t single_uid = 0x2000AFDE;
        seri.absorb(single_uid);
    }

    void sisregistry_client_subsession::request_sids(eka2l1::service::ipc_context *ctx) {
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