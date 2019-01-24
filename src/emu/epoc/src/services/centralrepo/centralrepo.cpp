#include <common/algorithm.h>
#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/e32inc.h>
#include <common/log.h>
#include <common/ini.h>

#include <epoc/services/centralrepo/centralrepo.h>
#include <epoc/services/centralrepo/cre.h>
#include <epoc/epoc.h>
#include <epoc/vfs.h>

#include <e32err.h>

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
        repo.owner_uid = crerep_owner->get<common::ini_value>(0)->get_as_native<std::uint32_t>();

        // Handle default metadata
        common::ini_section *crerep_default_meta = creini.find("defaultmeta")->get_as<common::ini_section>();
        
        if (crerep_default_meta) {
            for (auto &node: (*crerep_default_meta)) {
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

                    if (n && n->get_node_type() == common::INI_NODE_KEY && 
                        strncmp(n->name(), "mask", 4) == 0) {
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

        for (auto &node: (*main)) {
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
        REGISTER_IPC(central_repo_server, init, cen_rep_init, "CenRep::Init");
        REGISTER_IPC(central_repo_server, close, cen_rep_close, "CenRep::Close");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_reset, "CenRep::Reset");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_set_int, "CenRep::SetInt");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_notify_req, "CenRep::NofReq");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_group_nof_req, "CenRep::GroupNofReq");
    }

    void central_repo_server::init(service::ipc_context ctx) {
        // The UID repo to load
        const std::uint32_t repo_uid = static_cast<std::uint32_t>(*ctx.get_arg<int>(0));
        eka2l1::central_repo *repo = nullptr;

        auto ite = repos.find(repo_uid);

        if (ite != repos.end()) {
            // Load
            repo = &(ite->second);
        } else {
            repo = load_repo(ctx.sys->get_io_system(), repo_uid);

            if (!repo) {
                ctx.set_request_status(KErrNotFound);
                return;
            }
        }

        // New client session
        central_repo_client_session clisession;
        clisession.attach_repo = repo;
        clisession.server = this;

        const std::uint32_t id = ctx.msg->msg_session->unique_id();

        auto res = client_sessions.emplace(id, std::move(clisession));

        if (!res.second) {
            ctx.set_request_status(KErrNoMemory);
            return;
        }

        repo->attached.push_back(&res.first->second);

        bool result = ctx.write_arg_pkg<std::uint32_t>(3, id);
        ctx.set_request_status(KErrNone);
    }

    void central_repo_server::rescan_drives(eka2l1::io_system *io) {
        for (drive_number d = drive_z; d >= drive_a; d = static_cast<drive_number>(static_cast<int>(d) - 1)) {
            eka2l1::drive drv = std::move(*io->get_drive_entry(d));
            if (drv.media_type == drive_media::rom) {
                rom_drv = d;
            }

            if (static_cast<bool>(drv.attribute & io_attrib::internal) && !static_cast<bool>(drv.attribute & io_attrib::write_protected)) {
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

    void central_repo_server::redirect_msg_to_session(service::ipc_context ctx) {
        std::uint32_t session_uid = static_cast<std::uint32_t>(*ctx.get_arg<int>(3));
        auto session_ite = client_sessions.find(session_uid);

        if (session_ite == client_sessions.end()) {
            LOG_ERROR("Session ID passed not found 0x{:X}", session_uid);
            ctx.set_request_status(KErrArgument);

            return;
        }

        session_ite->second.handle_message(&ctx);
    }
    
    int central_repo_server::load_repo_adv(eka2l1::io_system *io, central_repo *repo, const std::uint32_t key,
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
        std::u16string rom_persists_dir { drive_to_char16(rom_drv) };
        rom_persists_dir += private_dir + repoini;

        bool one_on_rom = false;

        if (io->exist(rom_persists_dir)) {
            one_on_rom = true;
        }

        std::u16string private_dir_persists = u":\\Private\\10202be9\\persists\\";
    
        // Internal should only contains CRE
        for (auto &drv: avail_drives) {
            std::u16string repo_dir { drive_to_char16(drv) };
            std::u16string privates[2];
            
            if (scan_org_only) {
                privates[0] = private_dir;
            } else {
                privates[0] = private_dir_persists;
                privates[1] = private_dir;
            }

            for (int i = 0 ; i < ((one_on_rom) ? 1 : 2); i++) {
                std::u16string repo_folder = repo_dir + privates[i];

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

                        common::chunkyseri seri(&buf[0], common::SERI_MODE_READ);
                        if (int err = do_state_for_cre(seri, *repo)) {
                            LOG_ERROR("Loading CRE file failed with code: 0x{:X}, repo 0x{:X}", err, key);
                            return -1;
                        }

                        repo->reside_place = drv;

                        return 0;
                    }
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
                return 0;
            }
        }

        return -1;
    }
        
    /* It should be like follow:
     *
     * - The ROM INI are for rollback
     * - And repo initialsation file resides outside private/1020be9/*
     * 
     * That's for rollback when calling reset. Any changes in repo will be saved in persists folder
     * of preferable drive (usually internal).
    */
    eka2l1::central_repo *central_repo_server::load_repo(eka2l1::io_system *io, const std::uint32_t key) {
        eka2l1::central_repo repo;
        if (load_repo_adv(io, &repo, key, false) != 0) {
            return nullptr;
        }

        repos.emplace(key, std::move(repo));
        return &repos[key];
    }

    eka2l1::central_repo *central_repo_server::get_initial_repo(eka2l1::io_system *io, 
        const std::uint32_t key) {
        // Load from cache first
        eka2l1::central_repo *repo = backup_cacher.get_cached_repo(key);

        if (!repo) {
            // Load
            eka2l1::central_repo trepo;
            if (load_repo_adv(io, &trepo, key, true) != 0) {
                return nullptr;
            }

            return backup_cacher.add_repo(key, trepo);
        }

        return repo;
    }

    int central_repo_client_session::reset_key(eka2l1::central_repo *init_repo, const std::uint32_t key) {
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
        case cen_rep_group_nof_req: case cen_rep_notify_req: {
            const std::uint32_t mask = (ctx->msg->function == cen_rep_notify_req) ? 0xFFFFFFFF : 
                static_cast<std::uint32_t>(*ctx->get_arg<int>(1));

            const std::uint32_t partial_key = static_cast<std::uint32_t>(*ctx->get_arg<int>(0));

            epoc::notify_info info { ctx->msg->request_sts, ctx->msg->own_thr };
            const int err = add_notify_request(info, mask, partial_key);

            switch (err) {
            case 0: {
                ctx->set_request_status(KErrNone);
                break;
            }

            case -1: {
                ctx->set_request_status(KErrAlreadyExists);
                break;
            }

            default: {
                LOG_TRACE("Unknown returns code {} from add_notify_request, set status to KErrNone", err);
                ctx->set_request_status(KErrNone);

                break;
            }
            }

            break;
        }

        case cen_rep_set_int: {
            // We get the entry.
            // Use mode 1 (write) to get the entry, since we are modifying data.
            central_repo_entry *entry = get_entry(static_cast<std::uint32_t>(*ctx->get_arg<int>(0)), 1);

            // If it does not exist, or it is in different type, discard.
            // Depends on the invalid type, we set error code
            if (!entry) {
                ctx->set_request_status(KErrNotFound);
                break;
            }
            
            if (entry->data.etype != central_repo_entry_type::integer) {
                ctx->set_request_status(KErrArgument);
                break;
            }

            // TODO: Capability supply (+Policy)
            // This is really bad... We are not really care about accuracy right now
            // Assuming programs did right things, and accept the rules
            entry->data.intd = static_cast<std::uint64_t>(*ctx->get_arg<int>(1));
            // Success in modifying
            modification_success(entry->key);

            ctx->set_request_status(KErrNone);

            break;
        }

        case cen_rep_reset: { 
            io_system *io = ctx->sys->get_io_system();

            eka2l1::central_repo *init_repo = server->get_initial_repo(io, attach_repo->uid);

            // Reset the keys
            const std::uint32_t key = static_cast<std::uint32_t>(*ctx->get_arg<int>(0));
            int err = reset_key(init_repo, key);
            
            // In transaction
            if (err == -1) {
                ctx->set_request_status(KErrNotSupported);
                break;
            }

            if (err == -2) {
                ctx->set_request_status(KErrNotFound);
                break;
            }

            // Write committed changes to disk
            write_changes(io);
            modification_success(key);

            ctx->set_request_status(KErrNone);

            break;
        }

        default: {
            LOG_ERROR("Unhandled message opcode for cenrep 0x{:X]", ctx->msg->function);
            break;
        }
        }
    }

    int central_repo_server::closerep(io_system *io, const std::uint32_t repo_id, const std::uint32_t ss_id) {
        auto repo_session_ite = client_sessions.find(ss_id);

        if (repo_session_ite == client_sessions.end()) {
            return -1;
        }

        auto &repo_session = repo_session_ite->second;

        if (repo_id != 0 && repo_session.attach_repo->uid != repo_id) {
            LOG_CRITICAL("Fail safe check: REPO id != provided id");
            return -2;
        }

        // Sensei, did i do it correct
        // Save it and than wipe it out
        repo_session.write_changes(io);
        LOG_TRACE("Repo 0x{:X}: changes saved", repo_session.attach_repo->uid);

        // Bie...
        client_sessions.erase(repo_session_ite);
        return 0;
    }

    void central_repo_server::close(service::ipc_context ctx) {
        const int err = closerep(ctx.sys->get_io_system(), 0, ctx.msg->msg_session->unique_id());

        switch (err) {
        case 0: {
            ctx.set_request_status(KErrNone);
            break;
        }

        case -1: {
            ctx.set_request_status(KErrNotFound);
            break;
        }

        case -2: {
            ctx.set_request_status(KErrArgument);
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
    void central_repo_server::disconnect(service::ipc_context ctx) {
        closerep(ctx.sys->get_io_system(), 0, ctx.msg->msg_session->unique_id());

        // Ignore all errors
        ctx.set_request_status(KErrNone);
    }
}