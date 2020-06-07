/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <common/algorithm.h>
#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/ini.h>
#include <common/log.h>

#include <epoc/epoc.h>
#include <services/centralrepo/centralrepo.h>
#include <services/centralrepo/cre.h>
#include <services/context.h>
#include <manager/device_manager.h>
#include <manager/manager.h>

#include <utils/err.h>
#include <vfs/vfs.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace eka2l1 {
    // TODO: Security check. This include reading keyspace file (.cre) to get policies
    // information and reading capabilities section
    bool indentify_central_repo_entry_var_type(const std::string &tok, central_repo_entry_type &t) {
        if (tok == "int") {
            t = central_repo_entry_type::integer;
            return true;
        }

        if (tok == "string8") {
            t = central_repo_entry_type::string;
            return true;
        }

        if (tok == "string") {
            t = central_repo_entry_type::string;
            return true;
        }

        if (tok == "real") {
            t = central_repo_entry_type::real;
            return true;
        }

        if (tok == "binary") {
            t = central_repo_entry_type::string;
            return true;
        }

        return false;
    }

    bool parse_new_centrep_ini(const std::string &path, central_repo &repo) {
        common::ini_file creini;
        int err = creini.load(path.c_str());

        if (err < 0) {
            return false;
        }

        // Get the version
        if (!creini.find("cenrep")) {
            return false;
        }

        common::ini_pair *crerep_ver_info = creini.find("version")->get_as<common::ini_pair>();
        repo.ver = crerep_ver_info->get<common::ini_value>(0)->get_as_native<std::uint8_t>();

        // Get owner section
        common::ini_section *crerep_owner = creini.find("owner")->get_as<common::ini_section>();

        if (crerep_owner == nullptr) {
            repo.owner_uid = repo.uid;
        } else {
            repo.owner_uid = crerep_owner->get<common::ini_value>(0)->get_as_native<std::uint32_t>();
        }

        // Handle default metadata
        common::ini_section *crerep_default_meta = creini.find("defaultmeta")->get_as<common::ini_section>();

        if (crerep_default_meta) {
            for (auto &node : (*crerep_default_meta)) {
                switch (node->get_node_type()) {
                case common::INI_NODE_VALUE: {
                    // There is only a single value
                    // That should be the default metadata for everyone!!!
                    repo.default_meta = node->get_as<common::ini_value>()->get_as_native<std::uint32_t>();
                    break;
                }

                case common::INI_NODE_PAIR: {
                    common::ini_pair *p = node->get_as<common::ini_pair>();
                    central_repo_default_meta def_meta;

                    // Iterate through each elements to determine
                    std::uint32_t potentially_meta = p->key_as<std::uint32_t>();
                    common::ini_node *n = p->get<common::ini_node>(0);

                    if (n && n->get_node_type() == common::INI_NODE_KEY && strncmp(n->name(), "mask", 4) == 0) {
                        // That's a mask, we should add new
                        def_meta.default_meta_data = potentially_meta;
                        def_meta.key_mask = n->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_as_native<std::uint32_t>();
                        def_meta.low_key = p->get<common::ini_value>(1)->get_as_native<std::uint32_t>();
                    } else {
                        // Nope
                        def_meta.low_key = potentially_meta;
                        def_meta.high_key = n->get_as<common::ini_value>()->get_as_native<std::uint32_t>();
                        def_meta.default_meta_data = p->get<common::ini_value>(1)->get_as_native<std::uint32_t>();
                    }

                    repo.meta_range.push_back(def_meta);
                    break;
                }

                default: {
                    LOG_ERROR("Unsupported metadata INI node type");
                    return false;
                }
                }
            }
        }

        // TODO (pent0): Capability supply
        // Section: platsec

        // Main section
        common::ini_section *main = creini.find("Main")->get_as<common::ini_section>();
        // Iterate through each ini to get entry

        if (!main) {
            return false;
        }

        for (auto &node : (*main)) {
            common::ini_pair *p = node->get_as<common::ini_pair>();
            central_repo_entry entry;

            std::string key_num_str = p->name();

            if (key_num_str.length() > 2 && key_num_str[0] == '0' && (key_num_str[1] == 'x' || key_num_str[1] == 'X')) {
                key_num_str = key_num_str.substr(2, key_num_str.length() - 2);
                entry.key = static_cast<std::uint32_t>(std::strtoll(key_num_str.c_str(), nullptr, 16));
            } else {
                entry.key = static_cast<decltype(entry.key)>(std::atoi(key_num_str.c_str()));
            }

            std::string entry_type = p->get<common::ini_value>(0)->get_value();

            if (!indentify_central_repo_entry_var_type(entry_type, entry.data.etype)) {
                return false;
            }

            switch (entry.data.etype) {
            case central_repo_entry_type::integer: {
                entry.data.intd = p->get<common::ini_value>(1)->get_as_native<std::uint64_t>();
                break;
            }

            case central_repo_entry_type::string: {
                entry.data.strd = p->get<common::ini_value>(1)->get_value();
                break;
            }

            case central_repo_entry_type::real: {
                entry.data.reald = p->get<common::ini_value>(1)->get_as_native<double>();
                break;
            }

            default: {
                assert(false && "Unsupported data type!");
                break;
            }
            }

            if (p->get_value_count() >= 3) {
                entry.metadata_val = p->get<common::ini_value>(2)->get_as_native<std::uint32_t>();
                repo.add_new_entry(entry.key, entry.data, entry.metadata_val);
            } else {
                repo.add_new_entry(entry.key, entry.data);
            }

            // TODO (pent0): Capability supply
        }

        return true;
    }

    central_repo_server::central_repo_server(eka2l1::system *sys)
        : service::server(sys->get_kernel_system(), sys, CENTRAL_REPO_SERVER_NAME, true)
        , id_counter(0) {
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_init, "CenRep::Init");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_close, "CenRep::Close");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_reset, "CenRep::Reset");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_set_int, "CenRep::SetInt");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_set_string, "CenRep::SetStr");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_set_real, "CenRep::SetStr");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_notify_req, "CenRep::NofReq");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_group_nof_req, "CenRep::GroupNofReq");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_get_int, "CenRep::GetInt");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_get_real, "CenRep::GetReal");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_get_string, "CenRep::GetString");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_notify_req_check, "CenRep::NofReqCheck");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_find_eq_int, "CenRep::FindEqInt");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_find_neq_int, "CenRep::FindNeqInt");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_find, "CenRep::Find");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_get_find_res, "CenRep::GetFindResult");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_notify_cancel, "CenRep::NofCancel");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_notify_cancel_all, "CenRep::NofCancelAll");
    }

    void central_repo_client_session::init(service::ipc_context *ctx) {
        // The UID repo to load
        const std::uint32_t repo_uid = *ctx->get_arg<std::uint32_t>(0);
        manager::device_manager *mngr = ctx->sys->get_manager_system()->get_device_manager();
        eka2l1::central_repo *repo = server->load_repo_with_lookup(ctx->sys->get_io_system(), mngr, repo_uid);

        if (!repo) {
            LOG_TRACE("Repository not found with UID 0x{:X}", repo_uid);
            ctx->set_request_status(epoc::error_not_found);
            return;
        }

        // New client session
        central_repo_client_subsession clisubsession;
        clisubsession.attach_repo = repo;
        clisubsession.server = server;

        auto res = client_subsessions.emplace(++idcounter, std::move(clisubsession));

        if (!res.second) {
            ctx->set_request_status(epoc::error_no_memory);
            return;
        }

        repo->attached.push_back(&res.first->second);

        bool result = ctx->write_arg_pkg<std::uint32_t>(3, idcounter);
        ctx->set_request_status(epoc::error_none);
    }

    void central_repo_server::rescan_drives(eka2l1::io_system *io) {
        for (drive_number d = drive_z; d >= drive_a; d = static_cast<drive_number>(static_cast<int>(d) - 1)) {
            std::optional<eka2l1::drive> drv = io->get_drive_entry(d);

            if (!drv) {
                continue;
            }

            if (drv->media_type == drive_media::rom) {
                rom_drv = d;
            }

            if (static_cast<bool>(drv->attribute & io_attrib::internal) && !static_cast<bool>(drv->attribute & io_attrib::write_protected)) {
                avail_drives.push_back(d);
            }
        }
    }

    void central_repo_server::callback_on_drive_change(eka2l1::io_system *io, const drive_number drv, int act) {
        // Eject
        if (act == 0) {
            if (rom_drv == drv) {
                // Invalid
                rom_drv = drive_number::drive_count;
            } else {
                auto res = std::find(avail_drives.begin(), avail_drives.end(), drv);
                if (res != avail_drives.end()) {
                    avail_drives.erase(res);
                }
            }
        }

        // Mount
        if (act == 1) {
            eka2l1::drive drvi = std::move(*io->get_drive_entry(drv));
            if (drvi.media_type == drive_media::rom) {
                rom_drv = drv;
            }

            if (!static_cast<bool>(drvi.attribute & io_attrib::write_protected)) {
                avail_drives.push_back(drv);
            }
        }
    }

    void central_repo_server::redirect_msg_to_session(service::ipc_context &ctx) {
        const std::uint32_t session_uid = ctx.msg->msg_session->unique_id();
        auto session_ite = client_sessions.find(session_uid);

        if (session_ite == client_sessions.end()) {
            LOG_ERROR("Session ID passed not found 0x{:X}", session_uid);
            ctx.set_request_status(epoc::error_argument);

            return;
        }

        session_ite->second.handle_message(&ctx);
    }

    int central_repo_server::load_repo_adv(eka2l1::io_system *io, manager::device_manager *mngr, central_repo *repo, const std::uint32_t key,
        bool scan_org_only) {
        bool is_first_repo = first_repo;
        first_repo ? (first_repo = false) : 0;

        if (is_first_repo) {
            rescan_drives(io);
        }

        std::u16string keystr = common::utf8_to_ucs2(common::to_string(key, std::hex));

        // We prefer cre if it's available
        std::u16string repocre = keystr + u".CRE";
        std::u16string repoini = keystr + u".TXT";

        const std::u16string private_dir_persists = u":\\Private\\10202be9\\";
        const std::u16string firmcode = common::utf8_to_ucs2(common::lowercase_string(mngr->get_current()->firmware_code));
        const std::u16string private_dir_persists_separate_firm = private_dir_persists + u"persists\\" + firmcode + u"\\";

        // Temporary push rom drive so scan works
        avail_drives.push_back(rom_drv);

        // Internal should only contains CRE
        for (auto &drv : avail_drives) {
            std::u16string repo_dir{ drive_to_char16(drv) };

            // Don't add separate firmware code on rom drive (it already did itself)
            std::u16string repo_folder = repo_dir + ((drv == rom_drv) ? private_dir_persists : private_dir_persists_separate_firm);

            if (is_first_repo && !io->exist(repo_folder)) {
                // Create one if it doesn't exist, for the future
                io->create_directories(repo_folder);
            } else {
                // We can continue already
                std::u16string repo_path = repo_folder + repocre;

                if (io->exist(repo_path)) {
                    // Load and check for success
                    symfile repofile = io->open_file(repo_path, READ_MODE | BIN_MODE);

                    if (!repofile) {
                        LOG_ERROR("Found repo but open failed: {}", common::ucs2_to_utf8(repo_path));
                        avail_drives.pop_back();

                        return -1;
                    }

                    std::vector<std::uint8_t> buf;
                    buf.resize(repofile->size());

                    repofile->read_file(&buf[0], 1, static_cast<std::uint32_t>(buf.size()));
                    repofile->close();

                    common::chunkyseri seri(&buf[0], buf.size(), common::SERI_MODE_READ);

                    if (int err = do_state_for_cre(seri, *repo)) {
                        LOG_ERROR("Loading CRE file failed with code: 0x{:X}, repo 0x{:X}", err, key);
                        avail_drives.pop_back();

                        return -1;
                    }

                    repo->reside_place = avail_drives[0];
                    repo->access_count = 1;

                    avail_drives.pop_back();
                    return 0;
                }

                // Try to load the INI
                auto path = io->get_raw_path(repo_folder + repoini);

                if (!path) {
                    avail_drives.pop_back();
                    return -1;
                }

                repo->uid = key;
                if (parse_new_centrep_ini(common::ucs2_to_utf8(*path), *repo)) {
                    repo->reside_place = avail_drives[0];
                    repo->access_count = 1;
                    avail_drives.pop_back();

                    return 0;
                }
            }
        }

        avail_drives.pop_back();
        return -1;
    }

    /* It should be like follow:
     *
     * - The ROM INI are for rollback
     * - And repo initialisation file resides outside private/1020be9/
     * 
     * That's for rollback when calling reset. Any changes in repo will be saved in persists folder
     * of preferable drive (usually internal).
    */
    eka2l1::central_repo *central_repo_server::load_repo(eka2l1::io_system *io, manager::device_manager *mngr, const std::uint32_t key) {
        eka2l1::central_repo repo;
        if (load_repo_adv(io, mngr, &repo, key, false) != 0) {
            return nullptr;
        }

        repos.emplace(key, std::move(repo));
        return &repos[key];
    }

    eka2l1::central_repo *central_repo_server::load_repo_with_lookup(eka2l1::io_system *io, manager::device_manager *mngr, const std::uint32_t key) {
        auto result = repos.find(key);

        if (result != repos.end()) {
            result->second.access_count++;
            return &result->second;
        }

        return load_repo(io, mngr, key);
    }

    eka2l1::central_repo *central_repo_server::get_initial_repo(eka2l1::io_system *io,
        manager::device_manager *mngr, const std::uint32_t key) {
        // Load from cache first
        eka2l1::central_repo *repo = backup_cacher.get_cached_repo(key);

        if (!repo) {
            // Load
            eka2l1::central_repo trepo;
            if (load_repo_adv(io, mngr, &trepo, key, true) != 0) {
                return nullptr;
            }

            return backup_cacher.add_repo(key, trepo);
        }

        return repo;
    }

    void central_repo_client_session::handle_message(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case cen_rep_init: {
            init(ctx);
            break;
        }

        case cen_rep_close: {
            close(ctx);
            break;
        }

        default: {
            // We find the repo subsession and redirect message to subsession
            const std::uint32_t subsession_uid = *ctx->get_arg<std::uint32_t>(3);
            auto subsession_ite = client_subsessions.find(subsession_uid);

            if (subsession_ite == client_subsessions.end()) {
                LOG_ERROR("Subsession ID passed not found 0x{:X}", subsession_uid);
                ctx->set_request_status(epoc::error_argument);

                return;
            }

            subsession_ite->second.handle_message(ctx);
            break;
        }
        }
    }

    void central_repo_client_subsession::handle_message(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        // TODO: Faster way
        case cen_rep_notify_req_check:
            notify_nof_check(ctx);
            break;

        case cen_rep_group_nof_cancel:
        case cen_rep_notify_cancel:
            notify_cancel(ctx);
            break;

        case cen_rep_notify_cancel_all:
            cancel_all_notify_requests();
            ctx->set_request_status(epoc::error_none);
            break;

        case cen_rep_group_nof_req:
        case cen_rep_notify_req:
            notify(ctx);
            break;

        case cen_rep_get_int:
        case cen_rep_get_real:
        case cen_rep_get_string:
            get_value(ctx);
            break;

        case cen_rep_set_int:
        case cen_rep_set_string:
        case cen_rep_set_real:
            set_value(ctx);
            break;

        case cen_rep_reset:
            reset(ctx);
            break;

        case cen_rep_find_eq_int:
        case cen_rep_find_eq_real:
        case cen_rep_find_eq_string:
        case cen_rep_find_neq_int:
        case cen_rep_find_neq_real:
        case cen_rep_find_neq_string:
        case cen_rep_find: {
            find(ctx);
            break;
        }

        case cen_rep_get_find_res:
            get_find_result(ctx);
            break;

        default: {
            LOG_ERROR("Unhandled message opcode for cenrep 0x{:X]", ctx->msg->function);
            break;
        }
        }
    }

    int central_repo_client_session::closerep(io_system *io, manager::device_manager *mngr, const std::uint32_t repo_id, decltype(client_subsessions)::iterator repo_subsession_ite) {
        auto &repo_subsession = repo_subsession_ite->second;

        if (repo_id != 0 && repo_subsession.attach_repo->uid != repo_id) {
            LOG_CRITICAL("Fail safe check: REPO id != provided id");
            return -2;
        }

        // Sensei, did i do it correct
        // Save it and than wipe it out
        repo_subsession.write_changes(io, mngr);
        LOG_TRACE("Repo 0x{:X}: changes saved", repo_subsession.attach_repo->uid);

        // Remove from attach
        auto &all_attached = repo_subsession.attach_repo->attached;
        auto attach_this_ite = std::find(all_attached.begin(), all_attached.end(),
            &repo_subsession);

        if (attach_this_ite != all_attached.end()) {
            all_attached.erase(attach_this_ite);
        }

        // Decrease access count
        if (repo_subsession.attach_repo->access_count > 0) {
            repo_subsession.attach_repo->access_count--;
        } else {
            LOG_ERROR("Repo 0x{:X} has access count to be negative!", repo_subsession.attach_repo->uid);
        }

        // Bie...
        client_subsessions.erase(repo_subsession_ite);
        return 0;
    }

    int central_repo_client_session::closerep(io_system *io, manager::device_manager *mngr, const std::uint32_t repo_id, const std::uint32_t id) {
        auto repo_subsession_ite = client_subsessions.find(id);

        if (repo_subsession_ite == client_subsessions.end()) {
            return -1;
        }

        return closerep(io, mngr, repo_id, repo_subsession_ite);
    }

    void central_repo_client_session::close(service::ipc_context *ctx) {
        manager::device_manager *mngr = ctx->sys->get_manager_system()->get_device_manager();
        const int err = closerep(ctx->sys->get_io_system(), mngr, 0, *ctx->get_arg<std::uint32_t>(3));

        switch (err) {
        case 0: {
            ctx->set_request_status(epoc::error_none);
            break;
        }

        case -1: {
            ctx->set_request_status(epoc::error_not_found);
            break;
        }

        case -2: {
            ctx->set_request_status(epoc::error_argument);
            break;
        }

        default: {
            LOG_ERROR("Unknown return error from closerep {}", err);
            break;
        }
        }
    }

    // If a session disconnect, we should at least save all changes it did
    // At least, if the session connected still exist
    void central_repo_server::disconnect(service::ipc_context &ctx) {
        // Close all repos that are currently being opened.
        const std::uint32_t ss_id = ctx.msg->msg_session->unique_id();
        auto ss_ite = client_sessions.find(ss_id);

        io_system *io = ctx.sys->get_io_system();
        manager::device_manager *mngr = ctx.sys->get_manager_system()->get_device_manager();

        if (ss_ite != client_sessions.end()) {
            central_repo_client_session &ss = ss_ite->second;

            for (auto ite = ss.client_subsessions.begin(); ite != ss.client_subsessions.end(); ite++) {
                ss.closerep(io, mngr, 0, ite);
            }

            ss.client_subsessions.clear();
            client_sessions.erase(ss_ite);
        }

        // Ignore all errors
        ctx.set_request_status(epoc::error_none);
    }

    void central_repo_server::connect(service::ipc_context &ctx) {
        central_repo_client_session session;
        session.server = this;

        const std::uint32_t id = ctx.msg->msg_session->unique_id();

        // Put all process code here
        client_sessions.insert(std::make_pair(static_cast<const std::uint32_t>(id), std::move(session)));

        ctx.set_request_status(epoc::error_none);
    }
}
