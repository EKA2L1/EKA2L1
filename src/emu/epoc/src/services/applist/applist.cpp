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

#include <manager/manager.h>
#include <manager/package_manager.h>

#include <common/benchmark.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>

#include <epoc/epoc.h>
#include <epoc/vfs.h>
#include <epoc/loader/rsc.h>
#include <epoc/utils/bafl.h>

#include <functional>

#include <common/e32inc.h>
#include <e32err.h>
#include <e32lang.h>

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
            EAppListServGetAppCapability, "GetAppCapability")
    }

    bool applist_server::load_registry(eka2l1::io_system *io, const std::u16string &path, drive_number land_drive,
        const language ideal_lang) {
        // common::benchmarker marker(__FUNCTION__);

        apa_app_registry reg;

        // Load the resource
        symfile f = io->open_file(path, READ_MODE | BIN_MODE);

        if (!f) {
            return false;
        }

        auto read_rsc_from_file = [](symfile f, const int id, const bool confirm_sig, std::uint32_t *uid3) -> std::vector<std::uint8_t> {
            eka2l1::ro_file_stream std_rsc_raw(f);
            if (!std_rsc_raw.valid()) {
                return {};
            }

            loader::rsc_file std_rsc(reinterpret_cast<common::ro_stream*>(&std_rsc_raw));

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
        bool result = read_registeration_info(reinterpret_cast<common::ro_stream*>(&app_info_resource_stream),
            reg, land_drive);

        if (!result) {
            return false;
        }

        // Getting our localised resource info
        if (reg.localised_info_rsc_path.empty()) {
            // We will still do registeration. We have our mandatory info read fine.
            regs.emplace(reg.mandatory_info.uid, std::move(reg));
            return true;
        }

        const auto localised_path = utils::get_nearest_lang_file(io, reg.localised_info_rsc_path,
            ideal_lang, land_drive);

        if (localised_path.empty()) {
            // We will still do registeration. We have our mandatory info read fine.
            regs.emplace(reg.mandatory_info.uid, std::move(reg));
            return true;
        }

        // Read localised info
        // Ignore result
        read_localised_registeration_info(reinterpret_cast<common::ro_stream*>(&app_info_resource_stream),
            reg, land_drive);

        LOG_INFO("Found app: {}, uid: 0x{:X}", 
            common::ucs2_to_utf8(reg.mandatory_info.short_caption.to_std_string(nullptr)), 
            reg.mandatory_info.uid);

        regs.emplace(reg.mandatory_info.uid, std::move(reg));
        return true;
    }
    
    void applist_server::rescan_registries(eka2l1::io_system *io) {
        LOG_INFO("Loading app registries");

        for (drive_number drv = drive_z; drv >= drive_a; drv = static_cast<drive_number>(static_cast<int>(drv) - 1)) {
            if (io->get_drive_entry(drv)) {
                auto reg_dir = io->open_dir(std::u16string(1, drive_to_char16(drv)) + 
                    u":\\Private\\10003a3f\\import\\apps\\");

                if (reg_dir) {
                    while (auto ent = reg_dir->get_next_entry()) {
                        if (ent->type == io_component_type::file) {
                            load_registry(io, common::utf8_to_ucs2(ent->full_path), drv);
                        }
                    }
                }
            }
        }

        LOG_INFO("Done loading!");
    }
    
    void applist_server::connect(service::ipc_context &ctx) {
        server::connect(ctx);
    }

    std::map<std::uint32_t, apa_app_registry> &applist_server::get_registerations() {
        if (!(flags & AL_INITED)) {
            // Initialize
            rescan_registries(sys->get_io_system());
            flags |= AL_INITED;
        }
        
        return regs;
    }
    
    apa_app_registry *applist_server::get_registeration(const std::uint32_t uid) {
        if (!(flags & AL_INITED)) {
            // Initialize
            rescan_registries(sys->get_io_system());
            flags |= AL_INITED;
        }
        
        auto result = regs.find(uid);

        if (result == regs.end()) {
            return nullptr;
        }

        return &(result->second);
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
        std::uint32_t app_uid = *ctx.get_arg<int>(0);
        apa_app_registry *reg = get_registeration(app_uid);

        if (!reg) {
            ctx.set_request_status(KErrNotFound);
            return;
        }

        ctx.set_request_status(reg->default_screen_number);
    }

    void applist_server::app_language(service::ipc_context &ctx) {
        LOG_TRACE("AppList::AppLanguage stubbed to returns ELangEnglish");

        TLanguage default_lang = ELangEnglish;

        ctx.write_arg_pkg<TLanguage>(1, default_lang);
        ctx.set_request_status(0);
    }

    void applist_server::get_app_info(service::ipc_context &ctx) {
        std::uint32_t app_uid = *ctx.get_arg<int>(0);
        apa_app_registry *reg = get_registeration(app_uid);

        if (!reg) {
            ctx.set_request_status(KErrNotFound);
            return;
        }

        ctx.write_arg_pkg<apa_app_info>(1, reg->mandatory_info);
        ctx.set_request_status(KErrNone);
    }

    void applist_server::get_capability(service::ipc_context &ctx) {
        std::uint32_t app_uid = *ctx.get_arg<int>(1);
        apa_app_registry *reg = get_registeration(app_uid);

        if (!reg) {
            ctx.set_request_status(KErrNotFound);
            return;
        }

        ctx.write_arg_pkg<apa_capability>(0, reg->caps);
        ctx.set_request_status(KErrNone);
    }
}
