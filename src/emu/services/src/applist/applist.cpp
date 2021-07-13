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

#include <services/applist/applist.h>
#include <services/applist/op.h>
#include <services/fbs/fbs.h>
#include <services/context.h>

#include <common/benchmark.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/pystr.h>
#include <common/types.h>

#include <common/common.h>
#include <system/epoc.h>
#include <kernel/kernel.h>
#include <loader/rsc.h>
#include <utils/apacmd.h>
#include <utils/bafl.h>
#include <utils/des.h>
#include <vfs/vfs.h>

#include <utils/err.h>
#include <functional>

#include <config/config.h>

namespace eka2l1 {
    const std::string get_app_list_server_name_by_epocver(const epocver ver) {
        if (ver < epocver::epoc81a) {
            return "AppListServer";
        }

        return "!AppListServer";
    }

    static const char16_t *APA_APP_RUNNER = u"apprun.exe";

    applist_server::applist_server(system *sys)
        : service::typical_server(sys, get_app_list_server_name_by_epocver(sys->get_symbian_version_use()))
        , drive_change_handle_(0)
        , fbsserv(nullptr){
    }

    applist_server::~applist_server() {
        io_system *io = sys->get_io_system();

        for (const auto w: watchs_) {
            io->unwatch_directory(w);    
        }

        io->remove_drive_change_notify(drive_change_handle_);
    }
    
    bool applist_server::load_registry_oldarch(eka2l1::io_system *io, const std::u16string &path, drive_number land_drive,
        const language ideal_lang) {
        symfile f = io->open_file(path, READ_MODE | BIN_MODE);

        if (!f) {
            return false;
        }

        eka2l1::ro_file_stream std_rsc_raw(f.get());
        if (!std_rsc_raw.valid()) {
            LOG_ERROR(SERVICE_APPLIST, "Registry file {} is invalid!", common::ucs2_to_utf8(path));
            return false;
        }

        apa_app_registry reg;

        reg.land_drive = land_drive;
        reg.rsc_path = path;
        reg.mandatory_info.app_path = eka2l1::replace_extension(path, u".app");     // It seems so.

        if (!read_registeration_info_aif(reinterpret_cast<common::ro_stream*>(&std_rsc_raw), reg, land_drive, 
            ideal_lang)) {
            return false;
        }

        if (reg.mandatory_info.short_caption.get_length() == 0) {
            // Use the filename of the register file as the caption
            const std::u16string caption_to_use = eka2l1::replace_extension(eka2l1::filename(path, true),
                u"");

            reg.mandatory_info.short_caption.assign(nullptr, caption_to_use);
            reg.mandatory_info.long_caption.assign(nullptr, caption_to_use);
        }

        address romaddr = static_cast<address>(f->seek(0, file_seek_mode::address));

        if (romaddr == 0xFFFFFFFF) {
            romaddr = 0;
        }

        f->seek(0, file_seek_mode::beg);

        if (!read_icon_data_aif(reinterpret_cast<common::ro_stream*>(&std_rsc_raw), fbsserv, reg.app_icons,
            romaddr)) {
            return false;
        }

        std::u16string caption_file_path = eka2l1::replace_extension(path, u"") + u"_caption.rsc";
        caption_file_path = utils::get_nearest_lang_file(io, caption_file_path, ideal_lang, land_drive);
        
        f = io->open_file(caption_file_path, READ_MODE | BIN_MODE);

        if (f) {
            eka2l1::ro_file_stream caption_file_stream(f.get());
            if (!caption_file_stream.valid()) {
                LOG_INFO(SERVICE_APPLIST, "Caption file for {} is corrupted!", common::ucs2_to_utf8(reg.mandatory_info.short_caption.
                    to_std_string(nullptr)));
            } else {
                if (!read_caption_data_oldarch(reinterpret_cast<common::ro_stream*>(&caption_file_stream), reg)) {
                    LOG_INFO(SERVICE_APPLIST, "Failed to read caption file for {}", common::ucs2_to_utf8(reg.mandatory_info.short_caption.
                        to_std_string(nullptr)));
                }
            }
        }

        if (reg.mandatory_info.short_caption.get_length() == 0) {
            reg.caps.is_hidden = true;
        }
        
        regs.push_back(std::move(reg));
        return true;
    }

    bool applist_server::load_registry(eka2l1::io_system *io, const std::u16string &path, drive_number land_drive,
        const language ideal_lang) {
        // common::benchmarker marker(__FUNCTION__);
        const std::u16string nearest_path = utils::get_nearest_lang_file(io, path, ideal_lang, land_drive);

        auto find_result = std::find_if(regs.begin(), regs.end(), [=](const apa_app_registry &reg) {
            return reg.rsc_path == nearest_path;
        });

        if (find_result != regs.end()) {
            return true;
        }

        apa_app_registry reg;

        reg.land_drive = land_drive;
        reg.rsc_path = nearest_path;

        // Load the resource
        symfile f = io->open_file(nearest_path, READ_MODE | BIN_MODE);

        if (!f) {
            return false;
        }

        auto read_rsc_from_file = [](symfile &f, const int id, const bool confirm_sig, std::uint32_t *uid3) -> std::vector<std::uint8_t> {
            eka2l1::ro_file_stream std_rsc_raw(f.get());
            if (!std_rsc_raw.valid()) {
                return {};
            }

            loader::rsc_file std_rsc(reinterpret_cast<common::ro_stream *>(&std_rsc_raw));

            if (confirm_sig) {
                std_rsc.confirm_signature();
            }

            if (uid3) {
                *uid3 = std_rsc.get_uid(3);
            }

            return std_rsc.read(id);
        };

        // Open the file
        auto dat = read_rsc_from_file(f, 1, false, &reg.mandatory_info.uid);

        if (dat.empty()) {
            return false;
        }

        common::ro_buf_stream app_info_resource_stream(&dat[0], dat.size());
        bool result = read_registeration_info(reinterpret_cast<common::ro_stream *>(&app_info_resource_stream),
            reg, land_drive);

        if (!result) {
            return false;
        }

        // Getting our localised resource info
        if (reg.localised_info_rsc_path.empty()) {
            // Assume default path is used
            reg.localised_info_rsc_path.append(1, drive_to_char16(land_drive));
            reg.localised_info_rsc_path += u":\\resource\\apps\\";

            std::u16string name_rsc = eka2l1::replace_extension(eka2l1::filename(reg.rsc_path), u"");

            if (common::lowercase_ucs2_string(name_rsc.substr(name_rsc.length() - 4, 4)) == u"_reg") {
                name_rsc.erase(name_rsc.length() - 4, 4);
            }

            reg.localised_info_rsc_path += name_rsc + u".rsc";
        }

        // Absolute the localised info path
        reg.localised_info_rsc_path = eka2l1::absolute_path(reg.localised_info_rsc_path,
            eka2l1::root_dir(path), true);

        const auto localised_path = utils::get_nearest_lang_file(io, reg.localised_info_rsc_path,
            ideal_lang, land_drive);

        if (localised_path.empty()) {
            return true;
        }

        f = io->open_file(localised_path, READ_MODE | BIN_MODE);

        dat = read_rsc_from_file(f, reg.localised_info_rsc_id, true, nullptr);

        common::ro_buf_stream localised_app_info_resource_stream(&dat[0], dat.size());

        // Read localised info
        // Ignore result
        if (localised_app_info_resource_stream.valid()) {
            read_localised_registration_info(reinterpret_cast<common::ro_stream *>(&localised_app_info_resource_stream),
                reg, land_drive);
        }

        LOG_INFO(SERVICE_APPLIST, "Found app: {}, uid: 0x{:X}",
            common::ucs2_to_utf8(reg.mandatory_info.long_caption.to_std_string(nullptr)),
            reg.mandatory_info.uid);

        if (!eka2l1::is_absolute(reg.icon_file_path, std::u16string(u"c:\\"), true)) {
            // Try to absolute icon path
            // Search the registration file drive, and than the localizable registration file
            std::u16string try_1 = eka2l1::absolute_path(reg.icon_file_path,
                std::u16string(1, drive_to_char16(land_drive)) + u":\\", true);

            if (io->exist(try_1)) {
                reg.icon_file_path = try_1;
            } else {
                try_1[0] = localised_path[0];

                if (io->exist(try_1)) {
                    reg.icon_file_path = try_1;
                } else {
                    reg.icon_file_path = u"";
                }
            }
        }

        regs.push_back(std::move(reg));
        return true;
    }

    bool applist_server::delete_registry(const std::u16string &rsc_path) {
        auto result = std::find_if(regs.begin(), regs.end(), [rsc_path](const apa_app_registry &reg) {
            return common::compare_ignore_case(reg.rsc_path, rsc_path) == 0;
        });

        if (result == regs.end()) {
            return false;
        }

        regs.erase(result);
        return true;
    }

    void applist_server::sort_registry_list() {
        std::sort(regs.begin(), regs.end(), [](const apa_app_registry &lhs, const apa_app_registry &rhs) {
            return lhs.mandatory_info.uid < rhs.mandatory_info.uid;
        });
    }

    void applist_server::remove_registries_on_drive(const drive_number drv) {
        common::erase_elements(regs, [drv](const apa_app_registry &reg) {
            return reg.land_drive == drv;
        });
    }

    static const char *OLDARCH_REG_FILE_EXT = ".aif";
    static const char *NEWARCH_REG_FILE_EXT = ".r??";
    static const char16_t *NEWARCH_REG_FILE_SEARCH_WILDCARD16 = u"*.r??";

    void applist_server::on_register_directory_changes(eka2l1::io_system *io, const std::u16string &base, drive_number land_drive,
        common::directory_changes &changes) {
        const std::lock_guard<std::mutex> guard(list_access_mut_);

        for (auto &change : changes) {
            const std::string ext = eka2l1::path_extension(change.filename_);
            if ((legacy_level() < APA_LEGACY_LEVEL_MORDEN) && (common::compare_ignore_case(ext.c_str(), OLDARCH_REG_FILE_EXT) != 0)) {
                continue;
            } else if (legacy_level() == APA_LEGACY_LEVEL_MORDEN) {
                if (common::match_wildcard_in_string(ext, std::string(NEWARCH_REG_FILE_EXT), true) > 0) {
                    continue;
                }
            }

            const std::u16string rsc_path = eka2l1::add_path(base, common::utf8_to_ucs2(change.filename_));

            switch (change.change_) {
            case common::directory_change_action_created:
            case common::directory_change_action_moved_to:
                if (!change.filename_.empty()) {
                    if (legacy_level() < APA_LEGACY_LEVEL_MORDEN) {
                        load_registry_oldarch(io, rsc_path, land_drive);
                    } else {
                        load_registry(io, rsc_path, land_drive);
                    }
                }

                break;

            case common::directory_change_action_delete:
            case common::directory_change_action_moved_from:
                // Try to delete the app entry
                if (!change.filename_.empty()) {
                    delete_registry(rsc_path);
                }

                break;

            case common::directory_change_action_modified:
                // Delete the registry and then load it again
                delete_registry(rsc_path);

                if (legacy_level() < APA_LEGACY_LEVEL_MORDEN)
                    load_registry_oldarch(io, rsc_path, land_drive);
                else
                    load_registry(io, rsc_path, land_drive);

                break;

            default:
                break;
            }
        }

        sort_registry_list();
    }

    void applist_server::on_drive_change(void *userdata, drive_number drv, drive_action act) {
        io_system *io = reinterpret_cast<io_system*>(userdata);

        switch (act) {
        case drive_action_mount:
            if (kern->is_eka1()) {
                rescan_registries_on_drive_oldarch(io, drv);
            } else {
                rescan_registries_on_drive_newarch(io, drv);
            }

            sort_registry_list();
            break;

        case drive_action_unmount:
            remove_registries_on_drive(drv);
            break;

        default:
            break;
        }
    }

    void applist_server::rescan_registries_on_drive_oldarch(eka2l1::io_system *io, const drive_number drv) {
        const std::u16string base_dir = std::u16string(1, drive_to_char16(drv)) + u":\\System\\Apps\\";
        auto reg_dir = io->open_dir(base_dir, {}, io_attrib_include_dir);

        if (reg_dir) {
            while (auto ent = reg_dir->get_next_entry()) {
                if ((ent->type == io_component_type::dir) && (ent->name != ".") && (ent->name != "..")) {
                    const std::u16string aif_reg_file = common::utf8_to_ucs2(eka2l1::add_path(
                        ent->full_path, ent->name + OLDARCH_REG_FILE_EXT, true));

                    load_registry_oldarch(io, aif_reg_file, drv, kern->get_current_language());    
                }
            }
        }

        const std::int64_t watch = io->watch_directory(
            base_dir, [this, base_dir, io, drv](void *userdata, common::directory_changes &changes) {
                on_register_directory_changes(io, base_dir, drv, changes);
            },
            nullptr, common::directory_change_move | common::directory_change_last_write);

        if (watch != -1) {
            watchs_.push_back(watch);
        }
    }

    void applist_server::rescan_registries_on_drive_newarch(eka2l1::io_system *io, const drive_number drv) {
        const std::u16string base_dir = std::u16string(1, drive_to_char16(drv)) + u":\\Private\\10003a3f\\import\\apps\\";
        auto reg_dir = io->open_dir(base_dir + NEWARCH_REG_FILE_SEARCH_WILDCARD16, {}, io_attrib_include_file);

        if (reg_dir) {
            while (auto ent = reg_dir->get_next_entry()) {
                if (ent->type == io_component_type::file) {
                    load_registry(io, common::utf8_to_ucs2(ent->full_path), drv, kern->get_current_language());
                }
            }
        }

        const std::int64_t watch = io->watch_directory(
            base_dir, [this, base_dir, io, drv](void *userdata, common::directory_changes &changes) {
                on_register_directory_changes(io, base_dir, drv, changes);
            },
            nullptr, common::directory_change_move | common::directory_change_last_write);

        if (watch != -1) {
            watchs_.push_back(watch);
        }
    }

    void applist_server::rescan_registries(eka2l1::io_system *io) {        
        LOG_INFO(SERVICE_APPLIST, "Loading app registries");

        for (drive_number drv = drive_z; drv >= drive_a; drv--) {
            if (io->get_drive_entry(drv)) {
                if (kern->is_eka1()) {
                    rescan_registries_on_drive_oldarch(io, drv);
                } else {
                    rescan_registries_on_drive_newarch(io, drv);
                }
            }
        }

        sort_registry_list();

        // Register drive change callback
        drive_change_handle_ = io->register_drive_change_notify([this](void *userdata, drive_number drv, drive_action act) {
            return on_drive_change(userdata, drv, act);
        }, io);

        LOG_INFO(SERVICE_APPLIST, "Done loading!");
    }

    int applist_server::legacy_level() {
        if (kern->get_epoc_version() < epocver::epoc81a) {
            return APA_LEGACY_LEVEL_OLD;
        } else if (kern->get_epoc_version() < epocver::eka2) {
            return APA_LEGACY_LEVEL_TRANSITION;
        }

        return APA_LEGACY_LEVEL_MORDEN;
    }

    void applist_server::connect(service::ipc_context &ctx) {
        create_session<applist_session>(&ctx);
        server::connect(ctx);
    }

    void applist_server::init() {
        fbsserv = reinterpret_cast<fbs_server*>(kern->get_by_name<service::server>(
            epoc::get_fbs_server_name_by_epocver(kern->get_epoc_version())));

        rescan_registries(sys->get_io_system());
    
        flags |= AL_INITED;
    }

    std::vector<apa_app_registry> &applist_server::get_registerations() {
        const std::lock_guard<std::mutex> guard(list_access_mut_);

        if (!(flags & AL_INITED)) {
            // Initialize
            init();
        }

        return regs;
    }

    apa_app_registry *applist_server::get_registration(const std::uint32_t uid) {
        const std::lock_guard<std::mutex> guard(list_access_mut_);

        if (!(flags & AL_INITED)) {
            // Initialize
            init();
        }

        auto result = std::lower_bound(regs.begin(), regs.end(), uid, [](const apa_app_registry &lhs, const std::uint32_t rhs) {
            return lhs.mandatory_info.uid < rhs;
        });

        if (result != regs.end() && result->mandatory_info.uid == uid) {
            return &(*result);
        }

        return nullptr;
    }

    void applist_server::is_accepted_to_run(service::ipc_context &ctx) {
        auto exe_name = ctx.get_argument_value<std::u16string>(0);

        if (!exe_name) {
            ctx.complete(false);
            return;
        }

        LOG_TRACE(SERVICE_APPLIST, "Asking permission to launch: {}, accepted", common::ucs2_to_utf8(*exe_name));
        ctx.complete(true);
    }

    void applist_server::default_screen_number(service::ipc_context &ctx) {
        const epoc::uid app_uid = *ctx.get_argument_value<epoc::uid>(0);
        apa_app_registry *reg = get_registration(app_uid);

        if (!reg) {
            ctx.complete(epoc::error_not_found);
            return;
        }

        ctx.complete(reg->default_screen_number);
    }

    void applist_server::app_language(service::ipc_context &ctx) {
        LOG_TRACE(SERVICE_APPLIST, "AppList::AppLanguage stubbed to returns ELangEnglish");

        language default_lang = language::en;

        ctx.write_data_to_descriptor_argument<language>(1, default_lang);
        ctx.complete(0);
    }

    void applist_server::get_app_info(service::ipc_context &ctx) {
        const epoc::uid app_uid = *ctx.get_argument_value<epoc::uid>(0);
        apa_app_registry *reg = get_registration(app_uid);

        if (!reg) {
            ctx.complete(epoc::error_not_found);
            return;
        }

        ctx.write_data_to_descriptor_argument<apa_app_info>(1, reg->mandatory_info);
        ctx.complete(epoc::error_none);
    }

    void applist_server::get_capability(service::ipc_context &ctx) {
        const epoc::uid app_uid = *ctx.get_argument_value<epoc::uid>(1);
        apa_app_registry *reg = get_registration(app_uid);

        if (!reg) {
            ctx.complete(epoc::error_not_found);
            return;
        }

        ctx.write_data_to_descriptor_argument<apa_capability>(0, reg->caps, nullptr, true);
        ctx.complete(epoc::error_none);
    }

    void applist_server::get_app_icon_file_name(service::ipc_context &ctx) {
        const epoc::uid app_uid = *ctx.get_argument_value<epoc::uid>(0);
        apa_app_registry *reg = get_registration(app_uid);

        // Either the registeration doesn't exist, or the icon file doesn't exist
        if (!reg) {
            ctx.complete(epoc::error_not_found);
            return;
        }

        if (reg->icon_file_path.empty()) {
            ctx.complete(epoc::error_not_supported);
            return;
        }

        epoc::filename fname_des(reg->icon_file_path);

        ctx.write_data_to_descriptor_argument<epoc::filename>(1, fname_des);
        ctx.complete(epoc::error_none);
    }

    struct app_icon_handles {
        std::uint32_t bmp_handle;
        std::uint32_t mask_bmp_handle;
    };

    void applist_server::get_app_icon(service::ipc_context &ctx) {
        std::optional<epoc::uid> app_uid = ctx.get_argument_value<epoc::uid>(0);
        std::optional<std::int32_t> icon_size_width = std::nullopt;
        std::optional<std::int32_t> icon_size_height = std::nullopt;
        
        if (legacy_level() == APA_LEGACY_LEVEL_OLD) {
            std::optional<eka2l1::vec2> size_vec = ctx.get_argument_data_from_descriptor<eka2l1::vec2>(1);
            if (size_vec.has_value()) {
                icon_size_width = size_vec->x;
                icon_size_height = size_vec->y;
            }
        } else {
            icon_size_width = ctx.get_argument_value<std::int32_t>(1);
            icon_size_height = ctx.get_argument_value<std::int32_t>(2);
        }

        if (!app_uid || !icon_size_width || !icon_size_height) {
            ctx.complete(epoc::error_argument);
            return;
        }

        apa_app_registry *reg = get_registration(app_uid.value());

        if (!reg) {
            ctx.complete(epoc::error_not_found);
            return;
        }

        if (reg->app_icons.size() == 0) {
            // Usually it must have 1
            ctx.complete(epoc::error_not_supported);
            return;
        }

        app_icon_handles handle_result;

        for (std::size_t i = 0; i < reg->app_icons.size() / 2; i++) {
            if (reg->app_icons[i * 2].bmp_rom_addr_) {
                handle_result.bmp_handle = reg->app_icons[i * 2].bmp_rom_addr_;
            } else {
                handle_result.bmp_handle = reg->app_icons[i * 2].bmp_->id;
            }

            if (reg->app_icons[i * 2 + 1].bmp_rom_addr_) {
                handle_result.mask_bmp_handle = reg->app_icons[i * 2 + 1].bmp_rom_addr_;
            } else {
                handle_result.mask_bmp_handle = reg->app_icons[i * 2 + 1].bmp_->id;
            }
        }

        if (legacy_level() == APA_LEGACY_LEVEL_OLD) {
            ctx.write_data_to_descriptor_argument<std::uint32_t>(2, handle_result.bmp_handle);
            ctx.write_data_to_descriptor_argument<std::uint32_t>(3, handle_result.mask_bmp_handle);
        } else {
            ctx.write_data_to_descriptor_argument<app_icon_handles>(3, handle_result);
        }

        ctx.complete(epoc::error_none);
    }

    void applist_server::launch_app(service::ipc_context &ctx) {
        std::optional<std::u16string> cmd_line = ctx.get_argument_value<std::u16string>(0);
        if (!cmd_line) {
            LOG_ERROR(SERVICE_APPLIST, "Failed to launch a new app! Command line is not available.");
            ctx.complete(epoc::error_argument);

            return;
        }

        common::pystr16 cmd_line_py(cmd_line.value());
        auto arguments = cmd_line_py.split();

        if (arguments.empty()) {
            ctx.complete(epoc::error_argument);
            return;
        }

        // Simply load only
        codeseg_ptr seg = kern->get_lib_manager()->load(arguments[0].std_str());
        std::u16string app_launch = APA_APP_RUNNER;

        if (std::get<0>(seg->get_uids()) == epoc::EXECUTABLE_UID) {
            app_launch = arguments[0].std_str();
        }

        kernel::uid thread_id = 0;
        if (!launch_app(app_launch, cmd_line.value(), &thread_id, ctx.msg->own_thr->owning_process())) {
            LOG_ERROR(SERVICE_APPLIST, "Failed to create new app process (command line: {})", common::ucs2_to_utf8(cmd_line.value()));
            ctx.complete(epoc::error_no_memory);
            
            return;
        }

        if (kern->is_eka1()) {
            ctx.write_data_to_descriptor_argument<kernel::uid_eka1>(1, static_cast<kernel::uid_eka1>(thread_id));
        } else {
            ctx.write_data_to_descriptor_argument<kernel::uid>(1, thread_id);
        }

        ctx.complete(epoc::error_none);
    }

    void applist_server::is_program(service::ipc_context &ctx) {
        std::optional<std::u16string> path = ctx.get_argument_value<std::u16string>(0);

        if (!path.has_value()) {
            ctx.complete(epoc::error_argument);
            return;
        }
        
        hle::lib_manager *lmngr = kern->get_lib_manager();
        std::int32_t is_program = false;

        if (lmngr->load(path.value())) {
            is_program = true;
        }

        ctx.write_data_to_descriptor_argument<std::int32_t>(1, is_program);
        ctx.complete(epoc::error_none);
    }

    void applist_server::get_native_executable_name_if_non_native(service::ipc_context &ctx) {
        std::optional<std::u16string> path = ctx.get_argument_value<std::u16string>(1);

        if (!path.has_value()) {
            ctx.complete(epoc::error_argument);
            return;
        }
        
        hle::lib_manager *lmngr = kern->get_lib_manager();

        if (!lmngr->load(path.value())) {
            LOG_TRACE(SERVICE_APPLIST, "The file requested is non-native! Stubbed");
        }

        ctx.set_descriptor_argument_length(0, 0);

        LOG_TRACE(SERVICE_APPLIST, "No opaque data is written (stubbed).");
        ctx.complete(0);
    }

    void applist_server::app_info_provided_by_reg_file(service::ipc_context &ctx) {
        std::optional<epoc::uid> app_uid = ctx.get_argument_value<epoc::uid>(0);
        if (!app_uid.has_value()) {
            ctx.complete(epoc::error_argument);
            return;
        }

        apa_app_registry *reg = get_registration(app_uid.value());
        if (!reg) {
            ctx.complete(epoc::error_not_found);
            return;
        }

        const bool reg_file_available = !reg->rsc_path.empty();

        ctx.write_data_to_descriptor_argument(1, reg_file_available);
        ctx.complete(epoc::error_none);
    }

    void applist_server::get_preferred_buf_size(service::ipc_context &ctx) {
        LOG_TRACE(SERVICE_APPLIST, "AppList::GetPreferredBufSize stubbed to 0");

        std::uint32_t buf_size = 0;
        ctx.complete(buf_size);
    }

    void applist_server::get_app_for_document(service::ipc_context &ctx) {
        std::optional<std::u16string> path = ctx.get_argument_value<std::u16string>(2);
        if (!path.has_value()) {
            ctx.complete(epoc::error_argument);
            return;
        }

        applist_app_for_document app;
        app.uid = 0;
        app.data_type.uid = 0;

        config::state *conf = kern->get_config();

        if (conf && conf->mime_detection) {
            LOG_TRACE(SERVICE_APPLIST, "AppList::AppForDocument datatype stubbed with file extension");

            const std::u16string ext = eka2l1::path_extension(path.value());
            if (!ext.empty()) {
                app.data_type.data_type.assign(nullptr, common::uppercase_string(common::ucs2_to_utf8(ext.substr(1))));
            } else {
                app.data_type.data_type.assign(nullptr, "UNK");
            }
        } else {
            LOG_TRACE(SERVICE_APPLIST, "AppList::AppForDocument datatype left empty!");
        }

        ctx.write_data_to_descriptor_argument<applist_app_for_document>(0, app);
        ctx.complete(epoc::error_none);
    }

    applist_session::applist_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_ver)
        : typical_session(svr, client_ss_uid, client_ver) {
    }

    void applist_session::fetch(service::ipc_context *ctx) {
        const int llevel = server<applist_server>()->legacy_level();
        if (llevel == APA_LEGACY_LEVEL_OLD) {
            switch (ctx->msg->function) {
            case applist_request_oldarch_app_info:
                server<applist_server>()->get_app_info(*ctx);
                break;
    
            case applist_request_oldarch_start_app:
                server<applist_server>()->launch_app(*ctx);
                break;

            case applist_request_oldarch_is_program:
                server<applist_server>()->is_program(*ctx);
                break;

            case applist_request_oldarch_app_icon_by_uid_and_size:
                server<applist_server>()->get_app_icon(*ctx);
                break;

            default:
                LOG_ERROR(SERVICE_APPLIST, "Unimplemented applist opcode {}", ctx->msg->function);
                break;
            }
        } else if (llevel == APA_LEGACY_LEVEL_TRANSITION) {
            switch (ctx->msg->function) {
            case applist_request_trans_app_info:
                server<applist_server>()->get_app_info(*ctx);
                break;

            case applist_request_trans_app_capability:
                server<applist_server>()->get_capability(*ctx);
                break;

            case applist_request_trans_app_icon_filename:
                server<applist_server>()->get_app_icon_file_name(*ctx);
                break;

            case applist_request_trans_app_info_provide_by_reg_file:
                server<applist_server>()->app_info_provided_by_reg_file(*ctx);
                break;

            case applist_request_trans_app_icon_by_uid_and_size:
                server<applist_server>()->get_app_icon(*ctx);
                break;

            case applist_request_trans_start_app_without_returning_thread_id:
                server<applist_server>()->launch_app(*ctx);
                break;

            default:
                LOG_ERROR(SERVICE_APPLIST, "Unimplemented applist opcode 0x{:X}", ctx->msg->function);
                break;
            }
        } else {
            switch (ctx->msg->function) {
            case applist_request_get_default_screen_number:
                server<applist_server>()->default_screen_number(*ctx);
                break;

            case applist_request_app_language:
                server<applist_server>()->app_language(*ctx);
                break;

            case applist_request_rule_based_launching:
                server<applist_server>()->is_accepted_to_run(*ctx);
                break;

            case applist_request_app_info:
                server<applist_server>()->get_app_info(*ctx);
                break;

            case applist_request_app_capability:
                server<applist_server>()->get_capability(*ctx);
                break;

            case applist_request_app_icon_filename:
                server<applist_server>()->get_app_icon_file_name(*ctx);
                break;

            case applist_request_get_executable_name_if_non_native:
                server<applist_server>()->get_native_executable_name_if_non_native(*ctx);
                break;

            case applist_request_app_info_provide_by_reg_file:
                server<applist_server>()->app_info_provided_by_reg_file(*ctx);
                break;

            case applist_request_app_icon_by_uid_and_size:
                server<applist_server>()->get_app_icon(*ctx);
                break;

            case applist_request_preferred_buf_size:
                server<applist_server>()->get_preferred_buf_size(*ctx);
                break;

            case applist_request_app_for_document:
                server<applist_server>()->get_app_for_document(*ctx);
                break;

            default:
                LOG_ERROR(SERVICE_APPLIST, "Unimplemented applist opcode 0x{:X}", ctx->msg->function);
                break;
            }
        }
    }

    void apa_app_registry::get_launch_parameter(std::u16string &native_executable_path, epoc::apa::command_line &args) {
        const std::u16string app_path = mandatory_info.app_path.to_std_string(nullptr);

        if (caps.flags & apa_capability::built_as_dll) {
            native_executable_path = APA_APP_RUNNER;
        } else {
            native_executable_path = mandatory_info.app_path.to_std_string(nullptr);
        }

        args.document_name_ = eka2l1::replace_extension(eka2l1::filename(app_path), u"");
        args.executable_path_ = app_path;
        args.default_screen_number_ = default_screen_number;
    }

    static constexpr std::uint8_t ENVIRONMENT_SLOT_MAIN = 1;

    bool applist_server::launch_app(const std::u16string &exe_path, const std::u16string &cmd, kernel::uid *thread_id, kernel::process *requester) {
        process_ptr pr = kern->spawn_new_process(exe_path, (legacy_level() < APA_LEGACY_LEVEL_MORDEN) ? cmd : u"");

        if (!pr) {
            return false;
        }

        if (legacy_level() < APA_LEGACY_LEVEL_MORDEN) {
        }

        if (thread_id)
            *thread_id = pr->get_primary_thread()->unique_id();

        if (requester) {
            requester->add_child_process(pr);
        }

        // Add it into our app running list
        return pr->run();
    }

    bool applist_server::launch_app(apa_app_registry &registry, epoc::apa::command_line &parameter, kernel::uid *thread_id) {
        std::u16string executable_to_run;
        registry.get_launch_parameter(executable_to_run, parameter);

        std::u16string apacmddat = parameter.to_string(legacy_level() < APA_LEGACY_LEVEL_MORDEN);
        return launch_app(executable_to_run, apacmddat, thread_id);
    }

    std::optional<apa_app_masked_icon_bitmap> applist_server::get_icon(apa_app_registry &registry, const std::int8_t index) {
        epoc::bitwise_bitmap *real_bmp = nullptr;
        epoc::bitwise_bitmap *real_mask_bmp = nullptr;

        if (index * 2 >= registry.app_icons.size()) {
            return std::nullopt;
        }

        if (registry.app_icons[index * 2].bmp_)
            real_bmp = registry.app_icons[index * 2].bmp_->bitmap_;
        else
            real_bmp = eka2l1::ptr<epoc::bitwise_bitmap>(registry. app_icons[index * 2].bmp_rom_addr_).get(
                sys->get_memory_system());

        if (!real_bmp) {
            return std::nullopt;
        }
            
        if (registry.app_icons[index * 2 + 1].bmp_)
            real_mask_bmp = registry.app_icons[index * 2 + 1].bmp_->bitmap_;
        else
            real_mask_bmp = eka2l1::ptr<epoc::bitwise_bitmap>(registry.app_icons[index * 2 + 1].bmp_rom_addr_).get(
                sys->get_memory_system());

        return std::make_optional(std::make_pair(real_bmp, real_mask_bmp));
    }
}
