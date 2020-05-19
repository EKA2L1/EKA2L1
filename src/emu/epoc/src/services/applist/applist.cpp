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

#include <epoc/services/applist/applist.h>
#include <epoc/services/applist/op.h>

#include <common/benchmark.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/pystr.h>
#include <common/types.h>

#include <epoc/common.h>
#include <epoc/epoc.h>
#include <epoc/loader/rsc.h>
#include <epoc/utils/bafl.h>
#include <epoc/utils/des.h>
#include <epoc/vfs.h>

#include <epoc/utils/err.h>
#include <functional>

namespace eka2l1 {
    applist_server::applist_server(system *sys)
        : service::server(sys, "!AppListServer", true) {
        REGISTER_IPC(applist_server, default_screen_number,
            EAppListServGetDefaultScreenNumber, "GetDefaultScreenNumber");
        REGISTER_IPC(applist_server, app_language,
            EAppListServApplicationLanguage, "ApplicationLanguageL");
        REGISTER_IPC(applist_server, is_accepted_to_run,
            EAppListServRuleBasedLaunching, "RuleBasedLaunching");
        REGISTER_IPC(applist_server, get_app_info,
            EAppListServGetAppInfo, "GetAppInfo");
        REGISTER_IPC(applist_server, get_capability,
            EAppListServGetAppCapability, "GetAppCapability");
        REGISTER_IPC(applist_server, get_app_icon_file_name,
            EAppListServAppIconFileName, "GetAppIconFilename");
    }

    bool applist_server::load_registry(eka2l1::io_system *io, const std::u16string &path, drive_number land_drive,
        const language ideal_lang) {
        // common::benchmarker marker(__FUNCTION__);
        // Validate the path fits our current system
        if (!utils::is_file_compatible_with_language(common::ucs2_to_utf8(path), ".rsc", sys->get_system_language()))
            return false;

        apa_app_registry reg;
        reg.rsc_path = path;

        // Load the resource
        symfile f = io->open_file(path, READ_MODE | BIN_MODE);

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
            return true;
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
        read_localised_registration_info(reinterpret_cast<common::ro_stream *>(&localised_app_info_resource_stream),
            reg, land_drive);

        LOG_INFO("Found app: {}, uid: 0x{:X}",
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

    void applist_server::on_register_directory_changes(eka2l1::io_system *io, const std::u16string &base, drive_number land_drive,
        common::directory_changes &changes) {
        const std::lock_guard<std::mutex> guard(list_access_mut_);

        for (auto &change : changes) {
            const std::u16string rsc_path = eka2l1::add_path(base, common::utf8_to_ucs2(change.filename_));

            switch (change.change_) {
            case common::directory_change_action_created:
            case common::directory_change_action_moved_to:
                if (!change.filename_.empty())
                    load_registry(io, rsc_path, land_drive);

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
                load_registry(io, rsc_path, land_drive);

                break;

            default:
                break;
            }
        }

        sort_registry_list();
    }

    void applist_server::rescan_registries(eka2l1::io_system *io) {
        LOG_INFO("Loading app registries");

        for (drive_number drv = drive_z; drv >= drive_a; drv = static_cast<drive_number>(static_cast<int>(drv) - 1)) {
            if (io->get_drive_entry(drv)) {
                const std::u16string base_dir = std::u16string(1, drive_to_char16(drv)) + u":\\Private\\10003a3f\\import\\apps\\";
                auto reg_dir = io->open_dir(base_dir + u"*.r*", io_attrib::include_file);

                if (reg_dir) {
                    while (auto ent = reg_dir->get_next_entry()) {
                        if (ent->type == io_component_type::file) {
                            load_registry(io, common::utf8_to_ucs2(ent->full_path), drv);
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
        }

        sort_registry_list();
        LOG_INFO("Done loading!");
    }

    void applist_server::connect(service::ipc_context &ctx) {
        server::connect(ctx);
    }

    std::vector<apa_app_registry> &applist_server::get_registerations() {
        const std::lock_guard<std::mutex> guard(list_access_mut_);

        if (!(flags & AL_INITED)) {
            // Initialize
            rescan_registries(sys->get_io_system());
            flags |= AL_INITED;
        }

        return regs;
    }

    apa_app_registry *applist_server::get_registration(const std::uint32_t uid) {
        const std::lock_guard<std::mutex> guard(list_access_mut_);

        if (!(flags & AL_INITED)) {
            // Initialize
            rescan_registries(sys->get_io_system());
            flags |= AL_INITED;
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
        auto exe_name = ctx.get_arg<std::u16string>(0);

        if (!exe_name) {
            ctx.set_request_status(false);
            return;
        }

        LOG_TRACE("Asking permission to launch: {}, accepted", common::ucs2_to_utf8(*exe_name));
        ctx.set_request_status(true);
    }

    void applist_server::default_screen_number(service::ipc_context &ctx) {
        const epoc::uid app_uid = *ctx.get_arg<epoc::uid>(0);
        apa_app_registry *reg = get_registration(app_uid);

        if (!reg) {
            ctx.set_request_status(epoc::error_not_found);
            return;
        }

        ctx.set_request_status(reg->default_screen_number);
    }

    void applist_server::app_language(service::ipc_context &ctx) {
        LOG_TRACE("AppList::AppLanguage stubbed to returns ELangEnglish");

        language default_lang = language::en;

        ctx.write_arg_pkg<language>(1, default_lang);
        ctx.set_request_status(0);
    }

    void applist_server::get_app_info(service::ipc_context &ctx) {
        const epoc::uid app_uid = *ctx.get_arg<epoc::uid>(0);
        apa_app_registry *reg = get_registration(app_uid);

        if (!reg) {
            ctx.set_request_status(epoc::error_not_found);
            return;
        }

        ctx.write_arg_pkg<apa_app_info>(1, reg->mandatory_info);
        ctx.set_request_status(epoc::error_none);
    }

    void applist_server::get_capability(service::ipc_context &ctx) {
        const epoc::uid app_uid = *ctx.get_arg<epoc::uid>(1);
        apa_app_registry *reg = get_registration(app_uid);

        if (!reg) {
            ctx.set_request_status(epoc::error_not_found);
            return;
        }

        ctx.write_arg_pkg<apa_capability>(0, reg->caps);
        ctx.set_request_status(epoc::error_none);
    }

    void applist_server::get_app_icon_file_name(service::ipc_context &ctx) {
        const epoc::uid app_uid = *ctx.get_arg<epoc::uid>(0);
        apa_app_registry *reg = get_registration(app_uid);

        // Either the registeration doesn't exist, or the icon file doesn't exist
        if (!reg || reg->icon_file_path.empty()) {
            ctx.set_request_status(epoc::error_not_found);
            return;
        }

        epoc::filename fname_des(reg->icon_file_path);

        ctx.write_arg_pkg<epoc::filename>(1, fname_des);
        ctx.set_request_status(epoc::error_none);
    }
}
