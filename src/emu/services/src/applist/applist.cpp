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
#include <epoc/epoc.h>
#include <kernel/kernel.h>
#include <loader/rsc.h>
#include <utils/apacmd.h>
#include <utils/bafl.h>
#include <utils/des.h>
#include <vfs/vfs.h>

#include <utils/err.h>
#include <functional>

namespace eka2l1 {
    const std::string get_app_list_server_name_by_epocver(const epocver ver) {
        if (ver < epocver::eka2) {
            return "AppListServer";
        }

        return "!AppListServer";
    }

    applist_server::applist_server(system *sys)
        : service::typical_server(sys, get_app_list_server_name_by_epocver(sys->get_symbian_version_use()))
        , fbsserv(nullptr) {
    }

    bool applist_server::load_registry_oldarch(eka2l1::io_system *io, const std::u16string &path, drive_number land_drive,
        const language ideal_lang) {
        symfile f = io->open_file(path, READ_MODE | BIN_MODE);

        if (!f) {
            return false;
        }

        eka2l1::ro_file_stream std_rsc_raw(f.get());
        if (!std_rsc_raw.valid()) {
            LOG_ERROR("Registry file {} is invalid!", common::ucs2_to_utf8(path));
            return false;
        }

        apa_app_registry reg;
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

    void applist_server::rescan_registries_oldarch(eka2l1::io_system *io) {
        for (drive_number drv = drive_z; drv >= drive_a; drv--) {
            if (io->get_drive_entry(drv)) {
                const std::u16string base_dir = std::u16string(1, drive_to_char16(drv)) + u":\\System\\Apps\\";
                auto reg_dir = io->open_dir(base_dir, io_attrib_include_dir);

                if (reg_dir) {
                    while (auto ent = reg_dir->get_next_entry()) {
                        if (ent->type == io_component_type::dir) {
                            const std::u16string aif_reg_file = common::utf8_to_ucs2(eka2l1::add_path(
                                ent->full_path, ent->name + ".aif"));

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
        }
    }

    void applist_server::rescan_registries_newarch(eka2l1::io_system *io) {
        for (drive_number drv = drive_z; drv >= drive_a; drv--) {
            if (io->get_drive_entry(drv)) {
                const std::u16string base_dir = std::u16string(1, drive_to_char16(drv)) + u":\\Private\\10003a3f\\import\\apps\\";
                auto reg_dir = io->open_dir(base_dir + u"*.r*", io_attrib_include_file);

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
        }
    }

    void applist_server::rescan_registries(eka2l1::io_system *io) {        
        LOG_INFO("Loading app registries");

        if (kern->is_eka1()) {
            rescan_registries_oldarch(io);
        } else {
            rescan_registries_newarch(io);
        }

        sort_registry_list();
        LOG_INFO("Done loading!");
    }

    bool applist_server::is_oldarch() {
        return kern->is_eka1();
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

        LOG_TRACE("Asking permission to launch: {}, accepted", common::ucs2_to_utf8(*exe_name));
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
        LOG_TRACE("AppList::AppLanguage stubbed to returns ELangEnglish");

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

        ctx.write_data_to_descriptor_argument<apa_capability>(0, reg->caps);
        ctx.complete(epoc::error_none);
    }

    void applist_server::get_app_icon_file_name(service::ipc_context &ctx) {
        const epoc::uid app_uid = *ctx.get_argument_value<epoc::uid>(0);
        apa_app_registry *reg = get_registration(app_uid);

        // Either the registeration doesn't exist, or the icon file doesn't exist
        if (!reg || reg->icon_file_path.empty()) {
            ctx.complete(epoc::error_not_found);
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
        
        if (is_oldarch()) {
            icon_size_width = ctx.get_argument_data_from_descriptor<std::int32_t>(2);
            icon_size_height = ctx.get_argument_data_from_descriptor<std::int32_t>(3);
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

        ctx.write_data_to_descriptor_argument<app_icon_handles>(is_oldarch() ? 1 : 3, handle_result);
        ctx.complete(epoc::error_none);
    }

    applist_session::applist_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_ver)
        : typical_session(svr, client_ss_uid, client_ver) {
    }

    void applist_session::fetch(service::ipc_context *ctx) {
        if (server<applist_server>()->is_oldarch()) {
            switch (ctx->msg->function) {
            case applist_request_oldarch_app_info:
                server<applist_server>()->get_app_info(*ctx);
                break;
    
            case applist_request_oldarch_app_icon_by_uid_and_size:
                server<applist_server>()->get_app_icon(*ctx);
                break;

            default:
                LOG_ERROR("Unimplemented applist opcode {}", ctx->msg->function);
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

            default:
                LOG_ERROR("Unimplemented applist opcode 0x{:X}", ctx->msg->function);
                break;
            }
        }
    }

    void apa_app_registry::get_launch_parameter(std::u16string &native_executable_path, epoc::apa::command_line &args) {
        const std::u16string app_path = mandatory_info.app_path.to_std_string(nullptr);

        if (caps.flags & apa_capability::built_as_dll) {
            native_executable_path = u"apprun.exe";
        } else {
            native_executable_path = mandatory_info.app_path.to_std_string(nullptr);
        }

        args.document_name_ = eka2l1::replace_extension(eka2l1::filename(app_path), u"");
        args.executable_path_ = app_path;
        args.default_screen_number_ = default_screen_number;
    }

    static constexpr std::uint8_t ENVIRONMENT_SLOT_MAIN = 1;

    bool applist_server::launch_app(apa_app_registry &registry, epoc::apa::command_line &parameter) {
        std::u16string executable_to_run;
        registry.get_launch_parameter(executable_to_run, parameter);

        std::u16string apacmddat = parameter.to_string(is_oldarch());
        process_ptr pr = kern->spawn_new_process(executable_to_run, is_oldarch() ? apacmddat : u"");

        if (!pr) {
            return false;
        }

        if (!is_oldarch()) {
        }

        return pr->run();
    }
}
