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

#include <epoc/services/akn/skin/common.h>
#include <epoc/services/akn/skin/ops.h>
#include <epoc/services/akn/skin/server.h>
#include <epoc/services/akn/skin/skn.h>
#include <epoc/services/akn/skin/utils.h>
#include <epoc/services/fbs/fbs.h>
#include <epoc/vfs.h>

#include <common/cvt.h>
#include <common/log.h>
#include <epoc/utils/err.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>

#include <manager/manager.h>

namespace eka2l1 {
    akn_skin_server_session::akn_skin_server_session(service::typical_server *svr, service::uid client_ss_uid, epoc::version client_version)
        : service::typical_session(svr, client_ss_uid, client_version) {
    }

    void akn_skin_server_session::do_set_notify_handler(service::ipc_context *ctx) {
        // The notify handler does nothing rather than guarantee that the client already has a handle mechanic
        // to the request notification later.
        client_handler_ = *ctx->get_arg<std::uint32_t>(0);
        ctx->set_request_status(epoc::error_none);
    }

    void akn_skin_server_session::check_icon_config(service::ipc_context *ctx) {
        const std::optional<epoc::uid> id = ctx->get_arg_packed<epoc::uid>(0);

        if (!id) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        // Check if icon is configured
        ctx->set_request_status(server<akn_skin_server>()->is_icon_configured(id.value()));
    }

    void akn_skin_server_session::store_scalable_gfx(service::ipc_context *ctx) {
        // Message parameters
        // 0. ItemID
        // 1. LayoutType & size
        // 2. bitmap handle
        // 3. mask handle
        const std::optional<epoc::pid> item_id = ctx->get_arg_packed<epoc::pid>(0);
        const std::optional<epoc::skn_layout_info> layout_info = ctx->get_arg_packed<epoc::skn_layout_info>(1);
        const std::optional<std::int32_t> bmp_handle = ctx->get_arg<std::int32_t>(2);
        const std::optional<std::int32_t> msk_handle = ctx->get_arg<std::int32_t>(3);

        if (!(item_id && layout_info && bmp_handle && msk_handle)) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }
        server<akn_skin_server>()->store_scalable_gfx(
            *item_id,
            *layout_info,
            *bmp_handle,
            *msk_handle);

        ctx->set_request_status(epoc::error_none);
    }

    void akn_skin_server_session::do_next_event(service::ipc_context *ctx) {
        if (flags & ASS_FLAG_CANCELED) {
            // Clear the nof list
            while (!nof_list_.empty()) {
                nof_list_.pop();
            }

            // Set the notifier and both the next one getting the event to cancel
            ctx->set_request_status(epoc::error_cancel);
            nof_info_.complete(epoc::error_cancel);

            return;
        }

        // Check the notify list count
        if (nof_list_.size() > 0) {
            // The notification list is not empty.
            // Take the first element in the queue, and than signal the client with that code.
            const epoc::akn_skin_server_change_handler_notification nof_code = std::move(nof_list_.front());
            nof_list_.pop();

            ctx->set_request_status(static_cast<int>(nof_code));
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

        ctx->set_request_status(epoc::error_none);
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

        default: {
            LOG_ERROR("Unimplemented opcode: {}", epoc::akn_skin_server_opcode_to_str(static_cast<const epoc::akn_skin_server_opcode>(ctx->msg->function)));

            break;
        }
        }
    }

    akn_skin_server::akn_skin_server(eka2l1::system *sys)
        : service::typical_server(sys, "!AknSkinServer")
        , settings_(nullptr) {
    }

    void akn_skin_server::connect(service::ipc_context &ctx) {
        if (!settings_) {
            do_initialisation();
        }

        create_session<akn_skin_server_session>(&ctx);
        ctx.set_request_status(epoc::error_none);
    }

    int akn_skin_server::is_icon_configured(const epoc::uid app_uid) {
        return icon_config_map_->is_icon_configured(app_uid);
    }

    void akn_skin_server::merge_active_skin(eka2l1::io_system *io) {
        const epoc::pid skin_pid = settings_->active_skin_pid();
        const std::optional<std::u16string> skin_path = epoc::find_skin_file(io, skin_pid);
        const std::optional<std::u16string> resource_path = epoc::get_resource_path_of_skin(io, skin_pid);

        if (!skin_path.has_value()) {
            LOG_ERROR("Unable to find active skin file!");
            return;
        }

        symfile skin_file_obj = io->open_file(skin_path.value(), READ_MODE | BIN_MODE);
        eka2l1::ro_file_stream skin_file_stream(skin_file_obj.get());

        epoc::skn_file skin_parser(reinterpret_cast<common::ro_stream *>(&skin_file_stream));

        chunk_maintainer_->import(skin_parser, resource_path.value_or(u""));
    }

    void akn_skin_server::do_initialisation() {
        kernel_system *kern = sys->get_kernel_system();
        server_ptr svr = kern->get_by_name<service::server>("!CentralRepository");

        // Older versions dont use cenrep.
        settings_ = std::make_unique<epoc::akn_ss_settings>(sys->get_io_system(), !svr ? nullptr : reinterpret_cast<central_repo_server *>(&(*svr)));

        icon_config_map_ = std::make_unique<epoc::akn_skin_icon_config_map>(!svr ? nullptr : reinterpret_cast<central_repo_server *>(&(*svr)), sys->get_manager_system()->get_device_manager(),
            sys->get_io_system(), sys->get_system_language());

        fbss = reinterpret_cast<fbs_server *>(&(*kern->get_by_name<service::server>("!Fontbitmapserver")));

        // Create skin chunk
        skin_chunk_ = kern->create_and_add<kernel::chunk>(kernel::owner_type::kernel,
                              sys->get_memory_system(), nullptr, "AknsSrvSharedMemoryChunk",
                              0, 160 * 1024, 384 * 1024, prot::read_write, kernel::chunk_type::normal,
                              kernel::chunk_access::global, kernel::chunk_attrib::none)
                          .second;

        // Create semaphores and mutexes
        skin_chunk_sema_ = kern->create_and_add<kernel::semaphore>(kernel::owner_type::kernel,
                                   "AknsSrvWaitSemaphore", 127, kernel::access_type::global_access)
                               .second;

        // Render mutex. Use when render skins
        skin_chunk_render_mut_ = kern->create_and_add<kernel::mutex>(kernel::owner_type::kernel,
                                         sys->get_ntimer(), "AknsSrvRenderSemaphore", false,
                                         kernel::access_type::global_access)
                                     .second;

        std::uint32_t flags = 0;

        if (sys->get_symbian_version_use() > epocver::epoc94) {
            flags |= epoc::akn_skin_chunk_maintainer_lookup_use_linked_list;
        } else if (sys->get_symbian_version_use() == epocver::epoc94) {
            // Some Symbian version changes some binary struct.
            // It should be the new one if ... it is not in rom
            const std::u16string exe_path = u"z:\\sys\\bin\\aknskinsrv.exe";

            if (!sys->get_io_system()->is_entry_in_rom(exe_path)) {
                flags |= epoc::akn_skin_chunk_maintainer_lookup_use_linked_list;
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

        chunk_maintainer_->store_scalable_gfx(item_id, layout_info, bmp, msk);
    }
}
