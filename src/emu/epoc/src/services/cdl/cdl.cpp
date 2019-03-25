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

#include <epoc/services/cdl/cdl.h>
#include <epoc/services/cdl/ops.h>

#include <common/e32inc.h>
#include <common/chunkyseri.h>
#include <e32err.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>

namespace eka2l1 {
    cdl_server_session::cdl_server_session(service::typical_server *svr, service::uid client_ss_uid)
        : service::typical_session(svr, client_ss_uid) {
    }

    void cdl_server_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case epoc::cdl_server_cmd_notify_change: {
            notifier.requester = ctx->msg->own_thr;
            notifier.sts = ctx->msg->request_sts;

            break;
        }

        case epoc::cdl_server_cmd_get_temp_buf: {
            ctx->write_arg_pkg(0, reinterpret_cast<std::uint8_t*>(&temp_buf[0]), static_cast<std::uint32_t>(temp_buf.size()));
            ctx->set_request_status(KErrNone);

            break;
        }

        case epoc::cdl_server_cmd_get_refs_size: {            
            epoc::cdl_ref_collection filtered_col;

            // Subset by name
            if (*ctx->get_arg<int>(1)) {
                auto name_op = ctx->get_arg<std::u16string>(2);

                if (!name_op) {
                    ctx->set_request_status(KErrArgument);
                    return;
                }

                const std::u16string name = std::move(*name_op);

                // Get by name
                for (auto &ref_: server<cdl_server>()->collection_) {
                    if (ref_.name_ == name) {
                        filtered_col.push_back(ref_);
                    }
                }
            } else {
                // Get by UID
                const epoc::uid ref_uid = static_cast<const epoc::uid>(*ctx->get_arg<int>(3));

                // Get by name
                for (auto &ref_: server<cdl_server>()->collection_) {
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
                common::chunkyseri seri(reinterpret_cast<std::uint8_t*>(&temp_buf[0]), temp_buf.size(),
                    common::SERI_MODE_MEASURE);

                epoc::do_refs_state(seri, filtered_col);
            }

            ctx->write_arg_pkg<std::uint32_t>(0, static_cast<std::uint32_t>(temp_buf.size()));
            ctx->set_request_status(KErrNone);

            break;
        }

        default: {
            LOG_ERROR("Unimplemented IPC opcode for CDL server session: 0x{:X}", ctx->msg->function);
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
            &(*std::reinterpret_pointer_cast<ecom_server>(sys->get_kernel_system()->get_by_name<service::server>("!ecomserver"))),
            reinterpret_cast<epoc::cdl_ecom_watcher_observer*>(observer_.get()));
    }

    void cdl_server::connect(service::ipc_context ctx) {
        if (!observer_) {
            init();
        }

        create_session<cdl_server_session>(&ctx);
        ctx.set_request_status(KErrNone);
    }

    void cdl_server::add_refs(epoc::cdl_ref_collection &to_add_col_) {
        common::merge_and_replace(collection_, to_add_col_, [](const epoc::cdl_ref &lhs, const epoc::cdl_ref &rhs) {
            return lhs.uid_ == rhs.uid_;
        });
    }

    void cdl_server::remove_refs(const std::u16string &name) {
        common::erase_elements(collection_, [&](const epoc::cdl_ref &elem) { return elem.name_ == name; });
    }
}
