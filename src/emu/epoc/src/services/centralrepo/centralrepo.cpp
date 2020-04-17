#include <common/algorithm.h>
#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/ini.h>
#include <common/log.h>

#include <epoc/epoc.h>
#include <epoc/services/centralrepo/centralrepo.h>
#include <epoc/services/centralrepo/cre.h>
#include <epoc/vfs.h>
#include <manager/device_manager.h>
#include <manager/manager.h>

#include <epoc/utils/err.h>

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
                    std::uint32_t potientially_meta = p->key_as<std::uint32_t>();
                    common::ini_node *n = p->get<common::ini_node>(0);

                    if (n && n->get_node_type() == common::INI_NODE_KEY && strncmp(n->name(), "mask", 4) == 0) {
                        // That's a mask, we should add new
                        def_meta.default_meta_data = potientially_meta;
                        def_meta.key_mask = n->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_as_native<std::uint32_t>();
                        def_meta.low_key = p->get<common::ini_value>(1)->get_as_native<std::uint32_t>();
                    } else {
                        // Nope
                        def_meta.low_key = potientially_meta;
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
        : service::server(sys, "!CentralRepository", true)
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
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_notify_cancel_all, "CenRep::NofCancelAll");
    }

    void central_repo_client_session::init(service::ipc_context *ctx) {
        // The UID repo to load
        const std::uint32_t repo_uid = *ctx->get_arg<std::uint32_t>(0);
        manager::device_manager *mngr = ctx->sys->get_manager_system()->get_device_manager();
        eka2l1::central_repo *repo = server->load_repo_with_lookup(ctx->sys->get_io_system(), mngr, repo_uid);

        if (!repo) {
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

        std::u16string private_dir = u":\\Private\\10202be9\\";

        // Check for internal first, than fallback to ROM
        // Check if file exists on ROM first. If it's than it should resides in persists folder of internal drive
        std::u16string rom_persists_dir{ drive_to_char16(rom_drv) };
        rom_persists_dir += private_dir + repoini;

        bool one_on_rom = false;

        if (io->exist(rom_persists_dir)) {
            one_on_rom = true;
        }

        std::u16string private_dir_persists = u":\\Private\\10202be9\\persists\\";
        std::u16string firmcode = common::utf8_to_ucs2(common::lowercase_string(mngr->get_current()->firmware_code));

        private_dir_persists += firmcode + u"\\";

        // Internal should only contains CRE
        for (auto &drv : avail_drives) {
            if (drv == rom_drv)
                continue;

            std::u16string repo_dir{ drive_to_char16(drv) };
            std::u16string repo_folder = repo_dir + private_dir_persists;

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
                        return -1;
                    }

                    std::vector<std::uint8_t> buf;
                    buf.resize(repofile->size());

                    repofile->read_file(&buf[0], 1, static_cast<std::uint32_t>(buf.size()));
                    repofile->close();

                    common::chunkyseri seri(&buf[0], buf.size(), common::SERI_MODE_READ);
                    if (int err = do_state_for_cre(seri, *repo)) {
                        LOG_ERROR("Loading CRE file failed with code: 0x{:X}, repo 0x{:X}", err, key);
                        return -1;
                    }

                    repo->reside_place = drv;
                    repo->access_count = 1;

                    return 0;
                }
            }
        }

        if (one_on_rom) {
            // TODO: Make this kind of stuff rely in-memory
            auto path = io->get_raw_path(rom_persists_dir);

            if (!path) {
                LOG_ERROR("Can't get real path of {}!", common::ucs2_to_utf8(rom_persists_dir));
                return -1;
            }

            repo->uid = key;
            if (parse_new_centrep_ini(common::ucs2_to_utf8(*path), *repo)) {
                repo->reside_place = avail_drives[0];
                repo->access_count = 1;
                return 0;
            }
        }

        return -1;
    }

    /* It should be like follow:
     *
     * - The ROM INI are for rollback
     * - And repo initialsation file resides outside private/1020be9/
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

    int central_repo_client_subsession::reset_key(eka2l1::central_repo *init_repo, const std::uint32_t key) {
        // In transacton, fail
        if (is_active()) {
            return -1;
        }

        central_repo_entry *e = attach_repo->find_entry(key);

        if (!e) {
            return -2;
        }

        central_repo_entry *source_e = init_repo->find_entry(key);

        if (!source_e) {
            return -2;
        }

        e->data = source_e->data;
        e->metadata_val = source_e->metadata_val;

        return 0;
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
        case cen_rep_notify_req_check: {
            epoc::notify_info holder;

            if (add_notify_request(holder, 0xFFFFFFFF, *ctx->get_arg<std::int32_t>(0)) == 0) {
                ctx->set_request_status(epoc::error_none);
                break;
            }

            ctx->set_request_status(epoc::error_already_exists);
            break;
        }

        case cen_rep_group_nof_cancel:
        case cen_rep_notify_cancel: {
            const std::uint32_t mask = (ctx->msg->function == cen_rep_notify_req) ? 0xFFFFFFFF : *ctx->get_arg<std::uint32_t>(1);
            const std::uint32_t partial_key = *ctx->get_arg<std::uint32_t>(0);

            cancel_notify_request(partial_key, mask);

            ctx->set_request_status(epoc::error_none);
            break;
        }

        case cen_rep_notify_cancel_all: {
            cancel_all_notify_requests();

            ctx->set_request_status(epoc::error_none);
            break;
        }

        case cen_rep_group_nof_req:
        case cen_rep_notify_req: {
            const std::uint32_t mask = (ctx->msg->function == cen_rep_notify_req) ? 0xFFFFFFFF : *ctx->get_arg<std::uint32_t>(1);
            const std::uint32_t partial_key = *ctx->get_arg<std::uint32_t>(0);

            epoc::notify_info info{ ctx->msg->request_sts, ctx->msg->own_thr };
            const int err = add_notify_request(info, mask, partial_key);

            switch (err) {
            case 0: {
                break;
            }

            case -1: {
                ctx->set_request_status(epoc::error_already_exists);
                break;
            }

            default: {
                LOG_TRACE("Unknown returns code {} from add_notify_request, set status to epoc::error_none", err);
                ctx->set_request_status(epoc::error_none);

                break;
            }
            }

            break;
        }

        case cen_rep_get_int:
        case cen_rep_get_real:
        case cen_rep_get_string: {
            // We get the entry.
            // Use mode 0 (write) to get the entry, since we are modifying data.
            central_repo_entry *entry = get_entry(*ctx->get_arg<std::uint32_t>(0), 0);

            if (!entry) {
                ctx->set_request_status(epoc::error_not_found);
                break;
            }

            switch (ctx->msg->function) {
            case cen_rep_get_int: {
                if (entry->data.etype != central_repo_entry_type::integer) {
                    ctx->set_request_status(epoc::error_argument);
                    return;
                }

                int result = static_cast<int>(entry->data.intd);

                ctx->write_arg_pkg<int>(1, result);
                break;
            }

            case cen_rep_get_real: {
                if (entry->data.etype != central_repo_entry_type::real) {
                    ctx->set_request_status(epoc::error_argument);
                    return;
                }

                float result = static_cast<float>(entry->data.reald);

                ctx->write_arg_pkg<float>(1, result);
                break;
            }

            case cen_rep_get_string: {
                if (entry->data.etype != central_repo_entry_type::string) {
                    ctx->set_request_status(epoc::error_argument);
                    return;
                }

                ctx->write_arg_pkg(1, reinterpret_cast<std::uint8_t *>(&entry->data.strd[0]),
                    static_cast<std::uint32_t>(entry->data.strd.length()));
                break;
            }
            }

            ctx->set_request_status(epoc::error_none);
            break;
        }

        case cen_rep_set_int:
        case cen_rep_set_string:
        case cen_rep_set_real: {
            // We get the entry.
            // Use mode 1 (write) to get the entry, since we are modifying data.
            central_repo_entry *entry = get_entry(*ctx->get_arg<std::uint32_t>(0), 1);

            // If it does not exist, or it is in different type, discard.
            // Depends on the invalid type, we set error code
            if (!entry) {
                ctx->set_request_status(epoc::error_not_found);
                break;
            }

            // TODO: Capability supply (+Policy)
            // This is really bad... We are not really care about accuracy right now
            // Assuming programs did right things, and accept the rules
            switch (ctx->msg->function) {
            case cen_rep_set_int: {
                if (entry->data.etype != central_repo_entry_type::integer) {
                    ctx->set_request_status(epoc::error_argument);
                    break;
                }

                entry->data.intd = static_cast<std::uint64_t>(*ctx->get_arg<std::uint32_t>(1));
                break;
            }

            case cen_rep_set_real: {
                if (entry->data.etype != central_repo_entry_type::real) {
                    ctx->set_request_status(epoc::error_argument);
                    break;
                }

                entry->data.reald = *ctx->get_arg<float>(1);
                break;
            }

            case cen_rep_set_string: {
                if (entry->data.etype != central_repo_entry_type::string) {
                    ctx->set_request_status(epoc::error_argument);
                    break;
                }

                entry->data.strd = *ctx->get_arg<std::string>(1);
                break;
            }

            default: {
                // Unreachable
                break;
            }
            }

            // Success in modifying
            modification_success(entry->key);

            ctx->set_request_status(epoc::error_none);

            break;
        }

        case cen_rep_reset: {
            io_system *io = ctx->sys->get_io_system();
            manager::device_manager *mngr = ctx->sys->get_manager_system()->get_device_manager();

            eka2l1::central_repo *init_repo = server->get_initial_repo(io, mngr, attach_repo->uid);

            // Reset the keys
            const std::uint32_t key = *ctx->get_arg<std::uint32_t>(0);
            int err = reset_key(init_repo, key);

            // In transaction
            if (err == -1) {
                ctx->set_request_status(epoc::error_not_supported);
                break;
            }

            if (err == -2) {
                ctx->set_request_status(epoc::error_not_found);
                break;
            }

            // Write committed changes to disk
            write_changes(io, mngr);
            modification_success(key);

            ctx->set_request_status(epoc::error_none);

            break;
        }

        case cen_rep_find_eq_int:
        case cen_rep_find_eq_real:
        case cen_rep_find_eq_string: {
            find_eq(ctx);
            break;
        }

        default: {
            LOG_ERROR("Unhandled message opcode for cenrep 0x{:X]", ctx->msg->function);
            break;
        }
        }
    }

    void central_repo_client_subsession::append_new_key_to_found_eq_list(std::uint32_t *array, const std::uint32_t key) {
        // We have to push it to the temporary array, since this array can be retrieve anytime before another FindEq call
        // Even if the provided array is not full
        key_found_result.push_back(key);

        if (array[0] < MAX_FOUND_UID_BUF_LENGTH) {
            // Increase the length, than add the keey
            array[++array[0]] = key;
        }
    }

    void central_repo_client_subsession::find_eq(service::ipc_context *ctx) {
        // Clear found result
        // TODO: Should we?
        key_found_result.clear();

        // Get the filter
        std::optional<central_repo_key_filter> filter = ctx->get_arg_packed<central_repo_key_filter>(0);
        std::uint32_t *found_uid_result_array = ptr<std::uint32_t>(*ctx->get_arg<kernel::address>(2)).get(ctx->msg->own_thr->owning_process());

        if (!filter || !found_uid_result_array) {
            LOG_ERROR("Trying to find equal value in cenrep, but arguments are invalid!");
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        // Set found count to 0
        found_uid_result_array[0] = 0;

        for (auto &entry : attach_repo->entries) {
            // Try to match the key first
            if ((entry.key & filter->id_mask) != (filter->partial_key & filter->id_mask)) {
                // Mask doesn't match, abadon this entry
                continue;
            }

            std::uint32_t key_found = 0;
            bool find_not_eq = false;

            switch (ctx->msg->function) {
            case cen_rep_find_neq_int:
            case cen_rep_find_neq_real:
            case cen_rep_find_neq_string:
                find_not_eq = true;
                break;

            default:
                break;
            }

            // Depends on the opcode, we try to match the value
            switch (ctx->msg->function) {
            case cen_rep_find_eq_int:
            case cen_rep_find_neq_int: {
                if (entry.data.etype != central_repo_entry_type::integer) {
                    // It must be integer type
                    break;
                }

                // Index 1 argument contains the value we should look for
                // TODO: Signed/unsigned is dangerous
                if (static_cast<std::int32_t>(entry.data.intd) == *ctx->get_arg<std::int32_t>(1)) {
                    if (!find_not_eq) {
                        key_found = entry.key;
                    }
                } else {
                    if (find_not_eq) {
                        key_found = entry.key;
                    }
                }

                break;
            }

            default: {
                LOG_ERROR("Unimplement Cenrep's find function for opcode {}", ctx->msg->function);
                break;
            }
            }

            // If we found the key, append it
            if (key_found != 0) {
                append_new_key_to_found_eq_list(found_uid_result_array, key_found);
            }
        }

        ctx->set_request_status(epoc::error_none);
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
