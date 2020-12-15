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

#include <common/chunkyseri.h>

#include <system/epoc.h>
#include <kernel/kernel.h>
#include <services/cdl/cdl.h>
#include <services/cdl/ops.h>
#include <utils/err.h>

namespace eka2l1 {
    cdl_server_session::cdl_server_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_version)
        : service::typical_session(svr, client_ss_uid, client_version) {
    }

    void cdl_server_session::do_get_plugin_drive(service::ipc_context *ctx) {
        std::optional<std::u16string> name = ctx->get_argument_value<std::u16string>(0);

        if (!name) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const drive_number drv = server<cdl_server>()->watcher_->get_plugin_drive(name.value());

        if (drv == drive_invalid) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->complete(static_cast<int>(drv));
    }

    void cdl_server_session::do_get_refs_size(service::ipc_context *ctx) {
        epoc::cdl_ref_collection filtered_col;

        // Subset by name
        if (*ctx->get_argument_value<std::int32_t>(1)) {
            auto name_op = ctx->get_argument_value<std::u16string>(2);

            if (!name_op) {
                ctx->complete(epoc::error_argument);
                return;
            }

            const std::u16string name = std::move(*name_op);

            // Get by name
            for (auto &ref_ : server<cdl_server>()->collection_) {
                if (ref_.name_ == name) {
                    filtered_col.push_back(ref_);
                }
            }
        } else {
            // Get by UID
            const epoc::uid ref_uid = *ctx->get_argument_value<epoc::uid>(3);

            // Get by name
            for (auto &ref_ : server<cdl_server>()->collection_) {
                if (ref_.uid_ == ref_uid) {
                    filtered_col.push_back(ref_);
                }
            }
        }

        // Get size
        {
            common::chunkyseri seri(nullptr, 0, common::SERI_MODE_MEASURE);
            epoc::do_refs_state(seri, filtered_col);

            temp_buf.resize(seri.size());
        }

        {
            common::chunkyseri seri(reinterpret_cast<std::uint8_t *>(&temp_buf[0]), temp_buf.size(),
                common::SERI_MODE_WRITE);

            epoc::do_refs_state(seri, filtered_col);
        }

        std::uint32_t temp_buf_size = static_cast<std::uint32_t>(temp_buf.size());

        ctx->write_data_to_descriptor_argument<std::uint32_t>(0, temp_buf_size);
        ctx->complete(epoc::error_none);
    }

    void cdl_server_session::do_get_temp_buf(service::ipc_context *ctx) {
        ctx->write_data_to_descriptor_argument(0, reinterpret_cast<std::uint8_t *>(&temp_buf[0]), static_cast<std::uint32_t>(temp_buf.size()));
        ctx->complete(epoc::error_none);
    }

    void cdl_server_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case epoc::cdl_server_cmd_notify_change: {
            notifier.requester = ctx->msg->own_thr;
            notifier.sts = ctx->msg->request_sts;

            break;
        }

        case epoc::cdl_server_cmd_get_temp_buf: {
            do_get_temp_buf(ctx);
            break;
        }

        case epoc::cdl_server_cmd_get_refs_size: {
            do_get_refs_size(ctx);
            break;
        }

        case epoc::cdl_server_cmd_plugin_drive: {
            do_get_plugin_drive(ctx);
            break;
        }

        default: {
            LOG_ERROR(SERVICE_CDLENG, "Unimplemented IPC opcode for CDL server session: 0x{:X}", ctx->msg->function);
            break;
        }
        }
    }

    cdl_server::cdl_server(eka2l1::system *sys)
        : service::typical_server(sys, "CdlServer") {
    }

    void cdl_server::init() {
        observer_ = std::make_unique<epoc::cdl_ecom_generic_observer>(this);
        watcher_ = std::make_unique<epoc::cdl_ecom_watcher>(
            reinterpret_cast<ecom_server *>(sys->get_kernel_system()->get_by_name<service::server>("!ecomserver")),
            reinterpret_cast<epoc::cdl_ecom_watcher_observer *>(observer_.get()));
    }

    void cdl_server::connect(service::ipc_context &ctx) {
        if (!observer_) {
            init();
        }

        create_session<cdl_server_session>(&ctx);
        ctx.complete(epoc::error_none);
    }

    void cdl_server::add_refs(epoc::cdl_ref_collection &to_add_col_) {
        collection_.insert(collection_.end(), to_add_col_.begin(), to_add_col_.end());
    }

    void cdl_server::remove_refs(const std::u16string &name) {
        common::erase_elements(collection_, [&](const epoc::cdl_ref &elem) { return elem.name_ == name; });
    }
}
