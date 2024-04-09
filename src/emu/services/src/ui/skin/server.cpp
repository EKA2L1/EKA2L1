/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <services/fbs/fbs.h>
#include <services/ui/skin/common.h>
#include <services/ui/skin/ops.h>
#include <services/ui/skin/server.h>
#include <services/ui/skin/skn.h>
#include <services/ui/skin/utils.h>
#include <vfs/vfs.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/pystr.h>
#include <utils/err.h>

#include <kernel/kernel.h>
#include <system/epoc.h>
#include <loader/e32img.h>

namespace eka2l1 {
    static const epoc::pid DEFAULT_ALWAYS_EXIST_SKIN_PID = epoc::pid(0x101F84B9, 0);

    akn_skin_server_session::akn_skin_server_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_version)
        : service::typical_session(svr, client_ss_uid, client_version) {
    }

    void akn_skin_server_session::do_set_notify_handler(service::ipc_context *ctx) {
        // The notify handler does nothing rather than guarantee that the client already has a handle mechanic
        // to the request notification later.
        client_handler_ = *ctx->get_argument_value<std::uint32_t>(0);
        ctx->complete(epoc::error_none);
    }

    void akn_skin_server_session::check_icon_config(service::ipc_context *ctx) {
        const std::optional<epoc::uid> id = ctx->get_argument_data_from_descriptor<epoc::uid>(0);

        if (!id) {
            ctx->complete(epoc::error_argument);
            return;
        }

        // Check if icon is configured
        ctx->complete(server<akn_skin_server>()->is_icon_configured(id.value()));
    }

    void akn_skin_server_session::store_scalable_gfx(service::ipc_context *ctx) {
        // Message parameters
        // 0. ItemID
        // 1. LayoutType & size
        // 2. bitmap handle
        // 3. mask handle
        const std::optional<epoc::pid> item_id = ctx->get_argument_data_from_descriptor<epoc::pid>(0);
        const std::optional<epoc::skn_layout_info> layout_info = ctx->get_argument_data_from_descriptor<epoc::skn_layout_info>(1);
        const std::optional<std::int32_t> bmp_handle = ctx->get_argument_value<std::int32_t>(2);
        const std::optional<std::int32_t> msk_handle = ctx->get_argument_value<std::int32_t>(3);

        if (!(item_id && layout_info && bmp_handle && msk_handle)) {
            ctx->complete(epoc::error_argument);
            return;
        }
        server<akn_skin_server>()->store_scalable_gfx(
            *item_id,
            *layout_info,
            *bmp_handle,
            *msk_handle);

        ctx->complete(epoc::error_none);
    }

    void akn_skin_server_session::do_next_event(service::ipc_context *ctx) {
        if (flags & ASS_FLAG_CANCELED) {
            // Clear the nof list
            while (!nof_list_.empty()) {
                nof_list_.pop();
            }

            // Set the notifier and both the next one getting the event to cancel
            ctx->complete(epoc::error_cancel);
            nof_info_.complete(epoc::error_cancel);

            return;
        }

        // Check the notify list count
        if (nof_list_.size() > 0) {
            // The notification list is not empty.
            // Take the first element in the queue, and than signal the client with that code.
            const epoc::akn_skin_server_change_handler_notification nof_code = std::move(nof_list_.front());
            nof_list_.pop();

            ctx->complete(static_cast<int>(nof_code));
        } else {
            // There is no notification pending yet.
            // We have to wait for it, so let's get this client as the one to signal.
            nof_info_.requester = ctx->msg->own_thr;
            nof_info_.sts = ctx->msg->request_sts;
        }
    }

    void akn_skin_server_session::do_cancel(service::ipc_context *ctx) {
        // If a handler is set and no pending notifications
        if (client_handler_ && nof_list_.empty()) {
            nof_info_.complete(epoc::error_cancel);
        }

        ctx->complete(epoc::error_none);
    }

    void akn_skin_server_session::enumerate_packages(service::ipc_context *ctx) {
        package_list_.clear();

        auto scan_mode = ctx->get_argument_value<std::uint32_t>(1).value();
        auto scan_drive_bitmask = static_cast<std::uint32_t>(drive_c | drive_z);

        if (scan_mode == 4) {
            scan_drive_bitmask = 0xFFFFFFFF;
        } else if (scan_mode == 2) {
            scan_drive_bitmask |= static_cast<std::uint32_t>(drive_e);
        }

        auto io = ctx->sys->get_io_system();
        package_list_ = server<akn_skin_server>()->fresh_get_all_skin_infos(io, scan_drive_bitmask);

        std::uint32_t discovered_total = static_cast<std::uint32_t>(package_list_.size());

        ctx->write_data_to_descriptor_argument<std::uint32_t>(0, discovered_total);
        ctx->complete(epoc::error_none);
    }

    void akn_skin_server_session::retrieve_packages(service::ipc_context *ctx) {
        std::uint32_t package_buffer_capacity = ctx->get_argument_value<std::uint32_t>(0).value();
        std::uint32_t package_size = ctx->sys->get_symbian_version_use() < epocver::epoc95 ? epoc::SKIN_INFO_PACKAGE_SIZE_NO_ANIM_BG
             : epoc::SKIN_INFO_PACKAGE_SIZE_NORMAL;

        std::uint32_t bytes_copy = common::min(package_buffer_capacity, static_cast<std::uint32_t>(package_list_.size()))
            * package_size;

        if (ctx->get_argument_max_data_size(1) < bytes_copy) {
            ctx->complete(epoc::error_overflow);
            return;
        }

        auto ptr = ctx->get_descriptor_argument_ptr(1);
        for (const auto& package: package_list_) {
            std::memcpy(ptr, &package, package_size);
            ptr += package_size;
        }

        ctx->set_descriptor_argument_length(1, bytes_copy);
        ctx->complete(epoc::error_none);

        package_list_.clear();
    }

    void akn_skin_server_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case epoc::akn_skin_server_set_notify_handler: {
            do_set_notify_handler(ctx);
            break;
        }

        case epoc::akn_skin_server_next_event: {
            do_next_event(ctx);
            break;
        }

        case epoc::akn_skin_server_cancel: {
            do_cancel(ctx);
            break;
        }

        case epoc::akn_skin_server_store_scalable_gfx: {
            store_scalable_gfx(ctx);
            break;
        }

        case epoc::akn_skin_server_check_icon_config: {
            check_icon_config(ctx);
            break;
        }

        case epoc::akn_skin_server_enumerate_packages: {
            enumerate_packages(ctx);
            break;
        }

        case epoc::akn_skin_server_receive_packages: {
            retrieve_packages(ctx);
            break;
        }

        case epoc::akn_skin_server_disable_notify_skin_change: {
            // Stub
            LOG_TRACE(SERVICE_UI, "Disable skin change notify stubbed");

            ctx->complete(epoc::error_none);
            break;
        }

        default: {
            LOG_ERROR(SERVICE_UI, "Unimplemented opcode: {}", epoc::akn_skin_server_opcode_to_str(static_cast<const epoc::akn_skin_server_opcode>(ctx->msg->function)));

            break;
        }
        }
    }

    akn_skin_server::akn_skin_server(eka2l1::system *sys)
        : service::typical_server(sys, AKN_SKIN_SERVER_NAME)
        , settings_(nullptr)
        , skin_info_list_cached_(false) {
    }

    void akn_skin_server::connect(service::ipc_context &ctx) {
        if (!settings_) {
            do_initialisation_pre();
        }

        if (!chunk_maintainer_) {
            do_initialisation();
        }

        create_session<akn_skin_server_session>(&ctx);
        ctx.complete(epoc::error_none);
    }

    int akn_skin_server::is_icon_configured(const epoc::uid app_uid) {
        return icon_config_map_->is_icon_configured(app_uid);
    }

    void akn_skin_server::merge_active_skin(eka2l1::io_system *io) {
        epoc::pid skin_pid = settings_->active_skin_pid();
        if (skin_pid.first == 0) {
            epoc::pid default_pid = settings_->default_skin_pid();
            if (default_pid.first == 0) {
                // Pick one and save to setting
                std::optional<epoc::pid> picked = epoc::pick_first_skin(io);
                if (!picked) {
                    LOG_ERROR(SERVICE_UI, "Unable to pick a skin as default (no skin folder fits requirements)!");
                    return;
                }

                settings_->active_skin_pid(picked.value());
                settings_->default_skin_pid(picked.value());

                skin_pid = picked.value();
            } else {
                settings_->active_skin_pid(default_pid);
                skin_pid = default_pid;
            }
        }

        auto skn_file = get_skin(io, skin_pid);
        if (!skn_file) {
            skin_pid = DEFAULT_ALWAYS_EXIST_SKIN_PID;
            skn_file = get_skin(io, skin_pid);
            
            if (!skn_file) {
                // What?
                LOG_ERROR(SERVICE_UI, "Unable to find active skin file!");
                return;
            }
        }

        std::optional<std::u16string> resource_path = epoc::get_resource_path_of_skin(io, skin_pid);
        chunk_maintainer_->import(*skn_file, resource_path.value_or(u""));
    }

    void akn_skin_server::do_initialisation_pre() {
        kernel_system *kern = sys->get_kernel_system();
        server_ptr svr = kern->get_by_name<service::server>("!CentralRepository");

        // Older versions dont use cenrep.
        settings_ = std::make_unique<epoc::akn_ss_settings>(sys->get_io_system(), !svr ? nullptr : reinterpret_cast<central_repo_server *>(&(*svr)));

        icon_config_map_ = std::make_unique<epoc::akn_skin_icon_config_map>(!svr ? nullptr : reinterpret_cast<central_repo_server *>(&(*svr)),
            sys->get_device_manager(), sys->get_io_system(), sys->get_system_language());

        fbss = reinterpret_cast<fbs_server *>(&(*kern->get_by_name<service::server>(
            epoc::get_fbs_server_name_by_epocver(kern->get_epoc_version()))));
    }

    void akn_skin_server::do_initialisation() {
        // Create skin chunk
        skin_chunk_ = kern->create_and_add<kernel::chunk>(kernel::owner_type::kernel,
                              sys->get_memory_system(), nullptr, "AknsSrvSharedMemoryChunk",
                              0, 384 * 1024, 384 * 1024, prot_read_write, kernel::chunk_type::normal,
                              kernel::chunk_access::global, kernel::chunk_attrib::none)
                          .second;

        // Create semaphores and mutexes
        skin_chunk_sema_ = kern->create_and_add<kernel::semaphore>(kernel::owner_type::kernel,
                                   nullptr, "AknsSrvWaitSemaphore", 127, kernel::access_type::global_access)
                               .second;

        // Render mutex. Use when render skins
        skin_chunk_render_mut_ = kern->create_and_add<kernel::mutex>(kernel::owner_type::kernel,
                                         sys->get_ntimer(), nullptr, "AknsSrvRenderSemaphore", false,
                                         kernel::access_type::global_access)
                                     .second;

        std::uint32_t flags = 0;

        if (sys->get_symbian_version_use() > epocver::epoc94) {
            flags |= epoc::akn_skin_chunk_maintainer_lookup_use_linked_list;
        } else if (sys->get_symbian_version_use() == epocver::epoc94) {
            // Some Symbian version changes some binary struct.
            // It should be the new one if ... it is not in rom
            const std::u16string dll_path = u"z:\\sys\\bin\\aknskinsrv.dll";

            symfile dll_file = sys->get_io_system()->open_file(dll_path, READ_MODE | BIN_MODE | PREFER_PHYSICAL);
            if (dll_file) {
                ro_file_stream dll_file_stream(dll_file.get());
                if (loader::is_e32img(&dll_file_stream)) {
                    flags |= epoc::akn_skin_chunk_maintainer_lookup_use_linked_list;
                }
            }
        }

        // Create chunk maintainer
        chunk_maintainer_ = std::make_unique<epoc::akn_skin_chunk_maintainer>(skin_chunk_,
            4 * 1024, flags);

        // Merge the active skin as the first step
        merge_active_skin(sys->get_io_system());
    }

    void akn_skin_server::store_scalable_gfx(
        const epoc::pid item_id,
        const epoc::skn_layout_info layout_info,
        const std::uint32_t bmp_handle,
        const std::uint32_t msk_handle) {
        fbsbitmap *bmp = fbss->get<fbsbitmap>(bmp_handle);
        fbsbitmap *msk = msk_handle ? fbss->get<fbsbitmap>(msk_handle) : nullptr;

        bmp = bmp->final_clean();
        if (msk) {
            msk = msk->final_clean();
        }

        chunk_maintainer_->store_scalable_gfx(item_id, layout_info, bmp, msk);
    }

    const epoc::skn_file *akn_skin_server::get_skin(io_system *io, const epoc::pid skin_pid) {
        if (skin_file_cache_.find(skin_pid) != skin_file_cache_.end()) {
            return &skin_file_cache_.at(skin_pid);
        }

        std::optional<std::u16string> skin_path = epoc::find_skin_file(io, skin_pid);
        if (!skin_path.has_value()) {
            LOG_ERROR(SERVICE_UI, "Unable to find skin file with PID=(0x{:X}, 0x{:X}!", skin_pid.first, skin_pid.second);
            return nullptr;
        }

        symfile skin_file_obj = io->open_file(skin_path.value(), READ_MODE | BIN_MODE);
        eka2l1::ro_file_stream skin_file_stream(skin_file_obj.get());

        epoc::skn_file skin_parser(reinterpret_cast<common::ro_stream *>(&skin_file_stream));
        skin_file_cache_.emplace(skin_pid, std::move(skin_parser));

        return &skin_file_cache_.at(skin_pid);
    }

    const epoc::skn_file *akn_skin_server::get_active_skin(io_system *io) {
        if (!settings_) {
            do_initialisation_pre();
        }

        return get_skin(io, get_active_skin_pid(io));
    }

    epoc::pid akn_skin_server::get_active_skin_pid(io_system *io) {
        if (!settings_) {
            do_initialisation_pre();
        }

        auto active_skin_pid = settings_->active_skin_pid();
        if (active_skin_pid.first == 0) {
            active_skin_pid = settings_->default_skin_pid();

            if (active_skin_pid.first == 0) {
                auto pid_opt = epoc::pick_first_skin(io);
                if (pid_opt == std::nullopt) {
                    LOG_ERROR(SERVICE_UI, "Unable to pick a skin as default (no skin folder fits requirements)!");
                    return epoc::pid{ 0, 0 };
                } else {
                    active_skin_pid = pid_opt.value();
                }
            }

            // Set it
            settings_->active_skin_pid(active_skin_pid);
        }

        return active_skin_pid;
    }

    void akn_skin_server::set_active_skin_pid(const epoc::pid skin_pid) {
        if (!settings_) {
            do_initialisation_pre();
        }

        settings_->active_skin_pid(skin_pid);
    }

    std::vector<epoc::akn_skin_info_package> akn_skin_server::get_all_skin_infos() {
        if (!skin_info_list_cached_) {
            skin_info_cache_list_ = fresh_get_all_skin_infos(sys->get_io_system(), 0xFFFFFFFF);
            skin_info_list_cached_ = true;
        }

        return skin_info_cache_list_;
    }

    std::vector<epoc::akn_skin_info_package> akn_skin_server::fresh_get_all_skin_infos(io_system *io, const std::uint32_t drive_mask) {
        std::vector<epoc::akn_skin_info_package> package_list;
        auto list_files = epoc::find_skin_files(io, drive_mask);

        for (const auto& file_path: list_files) {
            auto file_handle = io->open_file(file_path, READ_MODE | BIN_MODE);
            if (!file_handle) {
                continue;
            }

            eka2l1::ro_file_stream skin_file_stream(file_handle.get());
            epoc::skn_file skin_parser(reinterpret_cast<common::ro_stream *>(&skin_file_stream), { 2, 8 },
                language::any, true);

            auto relative_dir = eka2l1::file_directory(file_path);
            auto relative_dir_iterator = eka2l1::basic_path_iterator(relative_dir);

            std::u16string pid_str;

            while (relative_dir_iterator) {
                pid_str = *relative_dir_iterator;
                relative_dir_iterator++;
            }

            common::pystr pid_converter(common::ucs2_to_utf8(pid_str));
            auto pid = pid_converter.as_int(0, 16);
            auto package_dir = epoc::get_resource_path_of_skin(io, { pid, 0 });

            // NOTE: This is stub
            // TODO: Remove the stub
            epoc::akn_skin_info_package package;
            package.full_name.assign(nullptr, skin_parser.skin_name_.name);
            package.skin_name_buf.assign(nullptr, skin_parser.skin_name_.name);
            package.pid_ = { pid, 0 };
            package.skin_directory_buf.assign(nullptr, package_dir.value_or(u""));
            package.skin_ini_file_directory_buf.assign(nullptr, u"=");
            package.idle_state_wall_paper_image_name.assign(nullptr, u"");
            package.is_copyable = 1;
            package.is_deletable = 1;
            package.corrupted = 0;
            package.color_scheme_id_ = { 0, 0 };
            package.protection_type = epoc::akn_skin_protection_none;

            package_list.push_back(package);
        }

        return package_list;
    }

    bool get_skin_icon(akn_skin_server *skin_server, io_system *io, const std::uint32_t app_uid, std::u16string &out_mif_path,
        std::uint32_t &out_index, std::uint32_t &out_mask_index) {
        if (skin_server->get_kernel_object_owner()->is_eka1()) {
            return false;
        }

        const epoc::skn_file *skin = skin_server->get_active_skin(io);

        if (!skin) {
            LOG_TRACE(SERVICE_UI, "No active skin found!");
            return false;
        }

        std::optional<std::u16string> resource_path = epoc::get_resource_path_of_skin(io, skin_server->get_active_skin_pid(io));
        if (!resource_path.has_value()) {
            LOG_TRACE(SERVICE_UI, "No resource path found for skin!");
            return false;
        }

        epoc::pid icon_table_pid{ epoc::AKN_SKIN_ICON_MAJOR_UID, app_uid };
        auto icon_table = skin->img_tabs_.find(static_cast<std::uint64_t>(icon_table_pid.second) << 32 | icon_table_pid.first);

        if (icon_table == skin->img_tabs_.end() || icon_table->second.images.empty()) {
            return false;
        }

        std::int32_t max_size = -1;
        std::size_t max_index = 0;

        // Grab the biggest image
        for (auto i = 0; i < icon_table->second.images.size(); i++) {
            auto img = skin->bitmaps_.find(icon_table->second.images[i]);
            if (img == skin->bitmaps_.end()) {
                continue;
            }

            if (max_size < 0 || img->second.attrib.image_size_x > max_size) {
                max_size = img->second.attrib.image_size_x;
                max_index = i;
            }
        }

        auto img = skin->bitmaps_.find(icon_table->second.images[max_index]);
        auto mif_path = add_path(resource_path.value(), skin->filenames_.at(img->second.filename_id));

        out_index = img->second.bmp_idx & ~0x4000;
        out_mask_index = img->second.mask_bitmap_idx & ~0x4000;
        out_mif_path = mif_path;

        return true;
    }
}
