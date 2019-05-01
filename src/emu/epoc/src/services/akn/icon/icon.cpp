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

#include <epoc/services/akn/icon/icon.h>
#include <epoc/services/akn/icon/ops.h>
#include <epoc/services/fbs/fbs.h>

#include <common/e32inc.h>
#include <e32err.h>

namespace eka2l1 {
    akn_icon_server_session::akn_icon_server_session(service::typical_server *svr, service::uid client_ss_uid) 
        : service::typical_session(svr, client_ss_uid) {
    }

    void akn_icon_server_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case akn_icon_server_get_init_data: {
            // Write initialisation data to buffer
            ctx->write_arg_pkg<epoc::akn_icon_init_data>(0, *server<akn_icon_server>()->get_init_data()
                , nullptr, true);
            ctx->set_request_status(KErrNone);
            
            break;
        }

        case akn_icon_server_retrieve_or_create_shared_icon: {
            server<akn_icon_server>()->retrieve_icon(ctx);
            break;
        }

        default: {
            LOG_ERROR("Unimplemented IPC opcode for AknIconServer session: 0x{:X}", ctx->msg->function);
            break;
        }
        }
    }

    akn_icon_server::akn_icon_server(eka2l1::system *sys)
        : service::typical_server(sys, "!AknIconServer") {
    }
    
    void akn_icon_server::connect(service::ipc_context &context) {
        if (!flags & akn_icon_srv_flag_inited) {
            init_server();
        }
        
        create_session<akn_icon_server_session>(&context);
        context.set_request_status(KErrNone);
    }

    void akn_icon_server::retrieve_icon(service::ipc_context *ctx) {
        std::optional<epoc::akn_icon_params> spec = ctx->get_arg_packed<epoc::akn_icon_params>(0);
        std::optional<epoc::akn_icon_srv_return_data> ret = ctx->get_arg_packed<epoc::akn_icon_srv_return_data>(1);

        if (!spec || !ret) {
            ctx->set_request_status(KErrArgument);
            return;
        }

        std::optional<epoc::akn_icon_srv_return_data> cached = find_cached_icon(spec.value());
        if (!cached) {
            eka2l1::vec2 size = spec->size;
            fbsbitmap *bmp = fbss->create_bitmap(size, init_data.icon_mode);
            fbsbitmap *mask = fbss->create_bitmap(size, init_data.icon_mask_mode);

            if (!bmp || !mask) {
                ctx->set_request_status(KErrNoMemory);
                return;
            }

            ret->bitmap_handle = bmp->id;
            ret->content_dim.x = size.x;
            ret->content_dim.y = size.y;
            ret->mask_handle = mask->id;

            add_cached_icon(ret.value(), spec.value());
        } else {
            ret.emplace(cached.value());
        }

        ctx->write_arg_pkg(0, spec.value());
        ctx->write_arg_pkg(1, ret.value());
        ctx->set_request_status(KErrNone);
    }

    std::optional<epoc::akn_icon_srv_return_data> akn_icon_server::find_cached_icon(epoc::akn_icon_params spec) {
        for (std::size_t i = 0; i < icons.size(); i++) {
            epoc::akn_icon_params cached_spec = icons[i].spec;

            if (spec.bitmap_id == cached_spec.bitmap_id && spec.mask_id == cached_spec.mask_id) {
                if (spec.flags == cached_spec.flags && spec.mode == cached_spec.mode) {
                    if (spec.size == cached_spec.size && spec.color == cached_spec.color) {
                        if (spec.file_name.to_std_string(nullptr) == cached_spec.file_name.to_std_string(nullptr)) {
                            return icons[i].ret;
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }

    void akn_icon_server::add_cached_icon(epoc::akn_icon_srv_return_data ret, epoc::akn_icon_params spec) {
        icon_cache item;
        item.ret = ret;
        item.spec = spec;
        if (icons.size() > MAX_CACHE_SIZE) {
            icons.erase(icons.begin());
        }
        icons.push_back(item);
    }
}
