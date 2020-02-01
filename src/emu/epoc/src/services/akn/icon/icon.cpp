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

#include <common/cvt.h>
#include <epoc/loader/mif.h>
#include <epoc/epoc.h>
#include <epoc/vfs.h>
#include <epoc/utils/err.h>

namespace eka2l1 {
    akn_icon_server_session::akn_icon_server_session(service::typical_server *svr, service::uid client_ss_uid, epoc::version version) 
        : service::typical_session(svr, client_ss_uid, version) {
    }

    void akn_icon_server_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case akn_icon_server_get_init_data: {
            // Write initialisation data to buffer
            ctx->write_arg_pkg<epoc::akn_icon_init_data>(0, *server<akn_icon_server>()->get_init_data()
                , nullptr, true);
            ctx->set_request_status(epoc::error_none);
            
            break;
        }

        case akn_icon_server_retrieve_or_create_shared_icon: {
            server<akn_icon_server>()->retrieve_icon(ctx);
            break;
        }

        case akn_icon_server_free_bitmap: {
            server<akn_icon_server>()->free_bitmap(ctx);
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
        context.set_request_status(epoc::error_none);
    }

    void akn_icon_server::retrieve_icon(service::ipc_context *ctx) {
        std::optional<epoc::akn_icon_params> spec = ctx->get_arg_packed<epoc::akn_icon_params>(0);
        std::optional<epoc::akn_icon_srv_return_data> ret = ctx->get_arg_packed<epoc::akn_icon_srv_return_data>(1);

        if (!spec || !ret) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        std::size_t icon_index = 0;
        std::optional<epoc::akn_icon_srv_return_data> cached = find_existing_icon(spec.value(), &icon_index);
        if (!cached) {
            eka2l1::vec2 size = spec->size;
            fbsbitmap *bmp = fbss->create_bitmap(size, init_data.icon_mode);
            fbsbitmap *mask = fbss->create_bitmap(size, init_data.icon_mask_mode);

            if (!bmp || !mask) {
                ctx->set_request_status(epoc::error_no_memory);
                return;
            }

            // Increase ref
            bmp->ref();
            mask->ref();

            ret->bitmap_handle = bmp->id;
            ret->content_dim.x = size.x;
            ret->content_dim.y = size.y;
            ret->mask_handle = mask->id;

            add_icon(ret.value(), spec.value());
        } else {
            ret.emplace(cached.value());
            icons[icon_index].use_count++;
        }

        ctx->write_arg_pkg(0, spec.value());
        ctx->write_arg_pkg(1, ret.value());
        ctx->set_request_status(epoc::error_none);
    }

    bool akn_icon_server::cache_or_delete_icon(const std::size_t icon_idx) {
        if (icon_idx >= icons.size()) {
            return false;
        }

        icon_data_item &icon = icons[icon_idx];

        // TODO: Cache
        fbsbitmap *original = fbss->get<fbsbitmap>(icon.ret.bitmap_handle);
        fbsbitmap *mask = fbss->get<fbsbitmap>(icon.ret.mask_handle);

        if (original) {
            // Try to free original bitmap, ignore result.
            original->count--;
            fbss->free_bitmap(original);
        }

        if (mask) {
            // Try to free mask bitmap, ignore result
            mask->count--;
            fbss->free_bitmap(mask);
        }

        // Delete the icon from the icon item list
        icons.erase(icons.begin() + icon_idx);

        return true;
    }

    void akn_icon_server::free_bitmap(service::ipc_context *ctx) {
        std::optional<epoc::akn_icon_params> params = ctx->get_arg_packed<epoc::akn_icon_params>(0);

        std::size_t icon_index = 0;
        if (!find_existing_icon(params.value(), &icon_index)) {
            // We can't find the icon. The params is fraud!!
            ctx->set_request_status(epoc::error_not_found);
            return;
        }

        // We have found the icon
        // Decrease the reference count
        icons[icon_index].use_count--;

        // If the reference count is 0, it means no one is using this bitmap anymore, for now. We have two options:
        // Cache the bitmap, or delete it from the server
        if (icons[icon_index].use_count == 0) {
            if (!cache_or_delete_icon(icon_index)) {
                ctx->set_request_status(epoc::error_general);
                return;
            }
        }

        // Success, return error none.
        ctx->set_request_status(epoc::error_none);
    }

    std::optional<epoc::akn_icon_srv_return_data> akn_icon_server::find_existing_icon(epoc::akn_icon_params &spec, std::size_t *idx) {
        for (std::size_t i = 0; i < icons.size(); i++) {
            epoc::akn_icon_params cached_spec = icons[i].spec;

            /**
             * The original implementation only check for the equal of:
             * - The bitmap ID.
             * - The bitmap container file (compare folded aka ignoring case)
             * - App icon?
             *
             * These three are the decesive elements that decide if two requests are trying to get the same bitmap.
             */
            if (spec.bitmap_id == cached_spec.bitmap_id) {
                if (common::compare_ignore_case(spec.file_name.to_std_string(nullptr), cached_spec.file_name.to_std_string(nullptr)) == 0) {
                    if (spec.app_icon == cached_spec.app_icon) {
                        if (idx) {
                            *idx = i;
                        }

                        return icons[i].ret;
                    }
                }
            }
        }
        return std::nullopt;
    }

    void akn_icon_server::add_icon(const epoc::akn_icon_srv_return_data &ret, const epoc::akn_icon_params &spec) {
        icon_data_item item;
        item.ret = ret;
        item.spec = spec;
        item.use_count = 1;

        icons.push_back(item);
    }
}
