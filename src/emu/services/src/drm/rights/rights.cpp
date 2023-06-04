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

#include <services/drm/rights/rights.h>
#include <services/drm/rights/import.h>
#include <services/fs/fs.h>
#include <system/epoc.h>
#include <vfs/vfs.h>
#include <utils/err.h>
#include <yaml-cpp/yaml.h>
#include <common/crypt.h>
#include <common/pystr.h>

namespace eka2l1 {
    static constexpr const char16_t *RIGHTS_FOLDER_PATH = u"C:\\Private\\101f51f2\\";
    static constexpr const char16_t *RIGHTS_DB_PATH = u"C:\\Private\\101f51f2\\rights.db";
    static constexpr const char16_t *IMPORT_DR_PATH = u":\\Private\\101f51f2\\import\\*.dr";
    static constexpr const char16_t *TEMP_FILE_PATH = u"C:\\system\\temp\\";
    static constexpr const std::uint32_t APP_INSTALLER_UID = 0x101F875A;

    rights_server::rights_server(eka2l1::system *sys)
        : service::typical_server(sys, RIGHTS_SERVER_NAME) {
    }

    std::uint32_t rights_server::get_suitable_seri_version() const {
        const epocver ver = sys->get_symbian_version_use();        
        if (ver <= epocver::epoc93fp1) {
            return 0;
        }

        return 1;
    }

    void rights_server::initialize() {
        io_system *io = sys->get_io_system();
        if (!io) {
            return;
        }

        io->create_directories(RIGHTS_FOLDER_PATH);

        std::optional<std::u16string> path = io->get_raw_path(RIGHTS_DB_PATH);
        if (!path.has_value()) {
            LOG_ERROR(SERVICE_DRMSYS, "No real path to rights DB available!");
            return;
        }

        database_ = std::make_unique<epoc::drm::rights_database>(path.value());
        startup_imports();
    }

    bool rights_server::import_ng2l(const std::string &content, std::vector<std::string> &success_game_name,
                                    std::vector<std::string> &failed_game_name) {
        if (!database_) {
            initialize();
        }

        success_game_name.clear();
        failed_game_name.clear();

        YAML::Node root = YAML::Load(content);
        if (root) {
            auto magic = root["ng2l"];
            if (!magic || magic.as<std::string>() != "ng2l") {
                return false;
            }

            auto games = root["games"];
            for (auto game_pair: games) {
                std::string game_name = game_pair.first.as<std::string>();
                YAML::Node game = game_pair.second;

                auto game_vid_node = game["vid"];
                auto game_sid_node = game["sid"];

                if (!game_vid_node || !game_sid_node) {
                    failed_game_name.push_back(game_name);
                    continue;
                }

                auto game_keys = game["keys"];
                if (!game_keys) {
                    failed_game_name.push_back(game_name);
                    continue;
                }

                std::uint32_t vid = 0;
                std::uint32_t sid = 0;

                try {
                    std::string vid_str = game_vid_node.as<std::string>();
                    std::string sid_str = game_sid_node.as<std::string>();

                    vid = common::pystr(vid_str).as_int(0, 16);
                    sid = common::pystr(sid_str).as_int(0, 16);

                    if ((vid == 0) || (sid == 0)) {
                        throw std::invalid_argument("VID or SID is not hex string!");
                    }
                } catch (std::exception &ex) {
                    failed_game_name.push_back(game_name);
                    continue;
                }

                epoc::drm::rights_permission permission_template;
                permission_template.version_rights_main_ = 1;
                permission_template.version_rights_sub_ = 0;
                permission_template.insert_time_ = common::get_current_utc_time_in_microseconds_since_0ad();
                permission_template.play_constraint_.active_constraints_ = epoc::drm::rights_constraint_vendor | epoc::drm::rights_constraint_software;
                permission_template.play_constraint_.vendor_id_ = vid;
                permission_template.play_constraint_.secure_id_ = sid;
                permission_template.available_rights_ = epoc::drm::rights_type_play;

                bool key_failed = false;

                std::vector<epoc::drm::rights_object> key_parsed;

                for (auto game_key: game_keys) {
                    epoc::drm::rights_object final_result;
                    final_result.common_data_.content_hash_.resize(20, 0);
                    final_result.permissions_.push_back(permission_template);

                    auto cid_node = game_key["cid"];
                    auto key_base64_node = game_key["value"];

                    if (!cid_node || !key_base64_node) {
                        key_failed = true;
                        break;
                    } else {
                        try {
                            final_result.common_data_.content_id_ = fmt::format("cid:{}@content.nokia.com", cid_node.as<std::uint32_t>());
                            std::string key_base64_str = key_base64_node.as<std::string>();

                            final_result.encrypt_key_.resize(16);
                            crypt::base64_decode(reinterpret_cast<const std::uint8_t*>(key_base64_str.c_str()), 24, final_result.encrypt_key_.data(), 16);
                        } catch (std::exception &ex)
                        {
                            key_failed = true;
                            break;
                        }
                    }

                    key_parsed.push_back(final_result);
                }

                if (key_failed) {
                    failed_game_name.push_back(game_name);
                    continue;
                }

                for (std::size_t i = 0; i < key_parsed.size(); i++) {
                    database_->add_or_update_record(key_parsed[i]);
                }

                success_game_name.push_back(game_name);
            }

            database_->flush();

            return true;
        }

        return false;
    }

    void rights_server::startup_imports() {
        io_system *io = sys->get_io_system();
        if (!io) {
            return;
        }

        std::u16string import_db_path_drv(1, '\0');
        import_db_path_drv += IMPORT_DR_PATH;

        // Intentionally skip drive Z
        for (drive_number drv = drive_a; drv < drive_z; drv++) {
            if (io->get_drive_entry(drv).has_value()) {
                import_db_path_drv[0] = drive_to_char16(drv);
                std::unique_ptr<eka2l1::directory> dir_vir = io->open_dir(import_db_path_drv);

                if (dir_vir) {
                    while (std::optional<eka2l1::entry_info> dr_entry = dir_vir->get_next_entry()) {
                        std::optional<std::u16string> dr_entry_real_path = io->get_raw_path(common::utf8_to_ucs2(dr_entry->full_path));
                        if (dr_entry_real_path) {
                            std::wstring dr_entry_real_wpath = common::ucs2_to_wstr(dr_entry_real_path.value());
                            pugi::xml_document document;
                            document.load_file(dr_entry_real_wpath.data());

                            if (document) {
                                if (std::optional<epoc::drm::rights_object> obj = epoc::drm::parse_dr_file(document)) {
                                    if (!database_->add_or_update_record(obj.value())) {
                                        LOG_ERROR(SERVICE_DRMSYS, "Fail to add/update right object with content ID: {}", obj->common_data_.content_id_);
                                    } else {
                                        io->delete_entry(common::utf8_to_ucs2(dr_entry->full_path));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        database_->flush();
    }

    void rights_server::connect(service::ipc_context &context) {
        if (!database_) {
            initialize();
        }

        create_session<rights_client_session>(&context);
        context.complete(epoc::error_none);
    }

    rights_client_session::rights_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version)
        , check_status_(rights_crediental_not_checked) {
    }

    void rights_client_session::get_entry_list(service::ipc_context *ctx) {
        std::optional<std::string> cid = ctx->get_argument_value<std::string>(1);

        if (!cid.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }
        
        kernel_system *kern = ctx->sys->get_kernel_system();
        fs_server *fss = kern->get_by_name<fs_server>(epoc::fs::get_server_name_through_epocver(kern->get_epoc_version()));

        if (!fss) {
            LOG_ERROR(SERVICE_DRMSYS, "Can't establish a connection to Rights server!");
            ctx->complete(epoc::error_general);
            return;
        }

        symfile temp = fss->get_temp_file(TEMP_FILE_PATH);
        if (!temp) {
            LOG_ERROR(SERVICE_DRMSYS, "Can't open a new temp file to write entry list!");
            ctx->complete(epoc::error_general);

            return;
        }

        std::vector<epoc::drm::rights_permission> perms;
        rights_server *rserv = server<rights_server>();

        if (!rserv->database().get_permission_list(cid.value(), perms)) {
            ctx->complete(ERROR_CA_NO_RIGHTS);
            return;
        }

        std::uint32_t target_version = rserv->get_suitable_seri_version();
        std::uint32_t perm_count = static_cast<std::uint32_t>(perms.size());

        temp->write_file(&perm_count, 1, 4);

        std::vector<std::uint8_t> data_to_write;

        for (std::size_t i = 0; i < perms.size(); i++) {
            common::chunkyseri seri(nullptr, 0, common::SERI_MODE_MEASURE);
            perms[i].absorb(seri, target_version);

            data_to_write.resize(seri.size());
            seri = common::chunkyseri(data_to_write.data(), data_to_write.size(), common::SERI_MODE_WRITE);

            perms[i].absorb(seri, target_version);

            temp->write_file(data_to_write.data(), 1, static_cast<std::uint32_t>(data_to_write.size()));
        }

        ctx->write_arg(0, temp->file_name());
        temp->close();

        ctx->complete(epoc::error_none);
    }

    void rights_client_session::init_key(service::ipc_context *ctx) {
        std::optional<std::string> cid = ctx->get_argument_value<std::string>(0);

        if (!cid.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        rights_server *rserv = server<rights_server>();
        if (!rserv->database().get_encryption_key(cid.value(), current_key_)) {
            current_key_.clear();
        }

        ctx->complete(epoc::error_none);
    }

    void rights_client_session::verify_crediental(kernel::process *client, const std::string &cid, const epoc::drm::rights_intent intent) {
        std::vector<epoc::drm::rights_permission> perms;
        if (!server<rights_server>()->database().get_permission_list(cid, perms)) {
            check_status_ = rights_crediental_checked_and_denied;
            return;
        }

        bool has_software_constraint = false;

        for (const epoc::drm::rights_permission &perm: perms) {
            if (perm.software_constrained()) {
                has_software_constraint = true;
            }
        }

        epoc::capability_set cap_check_drm;
        cap_check_drm.set(epoc::cap_drm);

        if ((client->has(cap_check_drm) && !has_software_constraint) || (client->get_sec_info().secure_id == APP_INSTALLER_UID)) {
            check_status_ = rights_crediental_checked_and_allowed;
            return;
        }

        bool is_accepted = false;

        // Get constraint. Just pick first RO for now. In the original source code, one must have permission and constraint, else
        // it will still not be accepted!
        if ((intent == epoc::drm::rights_intent_stop) || (intent == epoc::drm::rights_intent_pause) || (intent == epoc::drm::rights_intent_continue)) {
            // Whenever it is in any other state or not, we don't care at the moment :p
            is_accepted = true;
        } else {
            if (!perms.empty()) {
                epoc::drm::rights_constraint *constraint = perms[0].get_constraint_from_intent(intent);
                if (constraint->active_constraints_ & epoc::drm::rights_constraint_software) {
                    if (client->get_sec_info().secure_id == constraint->secure_id_) {
                        is_accepted = true;
                    }
                }

                if (constraint->active_constraints_ & epoc::drm::rights_constraint_vendor) {
                    if (client->get_sec_info().vendor_id == constraint->vendor_id_) {
                        is_accepted = true;
                    }
                }
            }
        }

        if (is_accepted) {
            check_status_ = rights_crediental_checked_and_allowed;
        }
    }

    void rights_client_session::consume(service::ipc_context *ctx) {
        epoc::drm::rights_intent intent = static_cast<epoc::drm::rights_intent>(ctx->msg->args.args[0]);
        std::optional<std::string> cid = ctx->get_argument_value<std::string>(1);

        if (!cid.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        verify_crediental(ctx->msg->own_thr->owning_process(), cid.value(), intent);
        ctx->complete(epoc::error_none);
    }

    void rights_client_session::get_key(service::ipc_context *ctx) {
        if (check_status_ == rights_crediental_checked_and_allowed) {
            if (current_key_.empty()) {
                ctx->complete(ERROR_CA_NO_RIGHTS);
            } else {
                ctx->write_data_to_descriptor_argument(2, reinterpret_cast<std::uint8_t*>(current_key_.data()), static_cast<std::uint32_t>(current_key_.size()));
                ctx->complete(epoc::error_none);
            }
        } else {
            ctx->complete(epoc::error_access_denied);
        }
    }

    void rights_client_session::check_consume(service::ipc_context *ctx) {
        std::optional<std::string> cid = ctx->get_argument_value<std::string>(1);
        if (!cid.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::drm::rights_intent intent = static_cast<epoc::drm::rights_intent>(ctx->msg->args.args[0]);
        if ((intent == epoc::drm::rights_intent_play) || (intent == epoc::drm::rights_intent_execute) || (intent == epoc::drm::rights_intent_view)
            || (intent == epoc::drm::rights_intent_print)) {
            if (server<rights_server>()->database().get_entry_id(cid.value()) < 0) {
                ctx->complete(ERROR_CA_NO_RIGHTS);
                return;
            }
        }

        ctx->complete(epoc::error_none);
    }

    void rights_client_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case rights_opcode_get_entry_list:
            get_entry_list(ctx);
            break;

        case rights_opcode_initialize_key:
            init_key(ctx);
            break;

        case rights_opcode_consume:
            consume(ctx);
            break;

        case rights_opcode_check_consume:
            check_consume(ctx);
            break;

        case rights_opcode_get_key:
            get_key(ctx);
            break;

        default:
            LOG_ERROR(SERVICE_DRMSYS, "Unimplemented opcode for RightsServer 0x{:X}", ctx->msg->function);
            ctx->complete(epoc::error_none);
        }
    }
}