/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <epoc/services/window/classes/gctx.h>
#include <epoc/services/window/classes/scrdvc.h>
#include <epoc/services/window/classes/wingroup.h>
#include <epoc/services/window/classes/winuser.h>
#include <epoc/services/window/op.h>
#include <epoc/services/window/opheader.h>
#include <epoc/services/window/window.h>
#include <epoc/services/window/screen.h>

#include <epoc/timing.h>

#include <common/log.h>
#include <common/vecx.h>

#include <epoc/utils/err.h>

namespace eka2l1::epoc {
    static constexpr std::uint8_t bits_per_ordpos = 4;
    static constexpr std::uint8_t max_ordpos_pri = 0b1111;
    static constexpr std::uint8_t max_pri_level = (sizeof(std::uint32_t) / bits_per_ordpos) - 1;

    // ======================= WINDOW USER BASE ======================================
    window_user_base::window_user_base(window_server_client_ptr client, screen *scr, window *parent, const window_kind kind)
         : window(client, scr, parent, kind) {
    }
    
    // ======================= WINDOW TOP USER (CLIENT) ===============================
    window_top_user::window_top_user(window_server_client_ptr client, screen *scr, window *parent)
        : window_user_base(client, scr, parent, window_kind::top_client) {

    }

    std::uint32_t window_top_user::redraw_priority(int *shift) {
        const std::uint32_t ordinal_pos = std::min<std::uint32_t>(max_ordpos_pri, ordinal_position(false));
        if (shift) {
            *shift = max_pri_level;
        }

        return (ordinal_pos << (max_pri_level * bits_per_ordpos));
    }

    eka2l1::vec2 window_top_user::get_origin() {
        return { 0, 0 };
    }

    window_user::window_user(window_server_client_ptr client, screen *scr, window *parent, const epoc::window_type type_of_window, const epoc::display_mode dmode, const std::uint32_t client_handle)
        : window_user_base(client, scr, parent, window_kind::client)
        , win_type(type_of_window)
        , pos(0, 0)
        , size(0, 0)
        , resize_needed(false) 
        , clear_color(0xFFFFFFFF)
        , filter(pointer_filter_type::all)
        , cursor_pos(-1, -1)
        , irect({ 0, 0 }, { 0, 0 })
        , dmode(dmode)
        , redraw_evt_id(0)
        , driver_win_id(0)
        , shadow_height(0)
        , flags(flags_visible) {
        if (parent->type != epoc::window_kind::top_client && parent->type != epoc::window_kind::client) {
            LOG_ERROR("Parent is not a window client type!");
        } else {
            if (parent->type == epoc::window_kind::client) {
                epoc::window_user *user = reinterpret_cast<epoc::window_user*>(parent);

                // Inherits parent's size
                pos = user->pos;
                size = user->size;
            } else {
                // Going fullscreen
                size = scr->size();
            }
        }

        set_client_handle(client_handle);
    }
    
    eka2l1::vec2 window_user::get_origin() {
        return pos;
    }
    
    void window_user::queue_event(const epoc::event &evt) {
        if (!is_visible()) {
            // TODO: Im not sure... I think it can certainly receive
            LOG_TRACE("The client window 0x{:X} is not visible, and can't receive any events", id);
            return;
        }

        window::queue_event(evt);
    }

    void window_user::set_extent(const eka2l1::vec2 &top, const eka2l1::vec2 &new_size) {        
        pos = top;

        // TODO (pent0): Currently our implementation differs from Symbian, that we invalidates the whole
        // window. The reason is because, when we are resizing the texture in OpenGL/Vulkan, there is
        // no easy way to keep buffer around without overheads. So Im just gonna make it redraw all.
        //
        // Where on Symbian, it only invalidates the reason which is empty when size is bigger.
        if (new_size != size) {
            // We do really need resize!
            resize_needed = true;
            client->queue_redraw(this, rect({ 0, 0 }, new_size));
        }

        size = new_size;
    }

    static bool should_purge_window_user(void *win, epoc::event &evt) {
        epoc::window_user *user = reinterpret_cast<epoc::window_user*>(win);
        if (user->client_handle == evt.handle) {
            return false;
        }

        return true;
    }

    void window_user::set_visible(const bool vis) {
        bool should_trigger_redraw = false;

        if (static_cast<bool>(flags & flags_visible) != vis) {
            should_trigger_redraw = true;
        }

        flags &= ~flags_visible;

        if (vis) {
            flags |= flags_visible;
        } else {
            // Purge all queued events now that the window is not visible anymore
            client->walk_event(should_purge_window_user, this);
        }

        if (should_trigger_redraw) {
            // Redraw the screen. NOW!
            client->get_ws().get_anim_scheduler()->schedule(client->get_ws().get_graphics_driver(),
                scr, client->get_ws().get_timing_system()->get_global_time_us());
        }
    }

    std::uint32_t window_user::redraw_priority(int *child_shift) {
        int ordpos = ordinal_position(false);
        if (ordpos > max_ordpos_pri) {
            ordpos = max_ordpos_pri;
        }

        int shift = 0;
        int parent_pri = reinterpret_cast<window_user_base*>(parent)->redraw_priority(&shift);

        if (shift > 0) {
            shift--;
        }

        if (child_shift) {
            *child_shift = shift;
        }

        return (parent_pri + (ordpos << (bits_per_ordpos * shift)));
    }

    void window_user::end_redraw(service::ipc_context &ctx, ws_cmd &cmd) {
        drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();

        if (resize_needed) {
            // Queue a resize command
            auto cmd_list = drv->new_command_list();
            auto cmd_builder = drv->new_command_builder(cmd_list.get());

            if (driver_win_id == 0) {
                driver_win_id = drivers::create_bitmap(drv, size);
            } else {
                cmd_builder->resize_bitmap(driver_win_id, size);
            }

            drv->submit_command_list(*cmd_list);

            resize_needed = false;
        }

        common::double_linked_queue_element *ite = attached_contexts.first();
        common::double_linked_queue_element *end = ite;

        // Set all contexts to be in recording
        do {
            if (!ite) {
                break;
            }

            epoc::graphic_context *ctx = E_LOFF(ite, epoc::graphic_context, context_attach_link);

            ctx->recording = false;
            ctx->flush_queue_to_driver();

            ite = ite->next;
        } while (ite != end);

        LOG_DEBUG("End redraw to window 0x{:X}!", id);

        // Want to trigger a screen redraw
        if (is_visible()) {
            client->get_ws().get_anim_scheduler()->schedule(client->get_ws().get_graphics_driver(),
                scr, client->get_ws().get_timing_system()->get_global_time_us());
        }

        ctx.set_request_status(epoc::error_none);
    }

    void window_user::begin_redraw(service::ipc_context &ctx, ws_cmd &cmd) {
        common::double_linked_queue_element *ite = attached_contexts.first();
        common::double_linked_queue_element *end = ite;

        // Set all contexts to be in recording
        do {
            if (!ite) {
                break;
            }

            E_LOFF(ite, epoc::graphic_context, context_attach_link)->recording = true;
            ite = ite->next;
        } while (ite != end);

        LOG_TRACE("Begin redraw to window 0x{:X}!", id);

        // Cancel pending redraw event, since by using this,
        // we already starts one
        if (redraw_evt_id) {
            client->deque_redraw(redraw_evt_id);
            redraw_evt_id = 0;
        }

        ctx.set_request_status(epoc::error_none);
    }

    void window_user::set_non_fading(service::ipc_context &context, ws_cmd &cmd) {
        const bool non_fading = *reinterpret_cast<bool*>(cmd.data_ptr);

        if (non_fading) {
            flags |= flags_non_fading;
        } else {
            flags &= ~(flags_non_fading);
        }
    }

    void window_user::set_size(service::ipc_context &context, ws_cmd &cmd) {
        // refer to window_user::set_extent()
        const object_size new_size = *reinterpret_cast<object_size*>(cmd.data_ptr);

        if (new_size != size) {
            resize_needed = true;
            client->queue_redraw(this, rect({ 0, 0 }, new_size));
        }

        size = new_size;
        context.set_request_status(epoc::error_none);
    }

    bool window_user::clear_redraw_store() {
        return true;
    }

    void window_user::set_fade(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_set_fade *fade_param = reinterpret_cast<ws_cmd_set_fade*>(cmd.data_ptr);
        LOG_TRACE("Set fade stubbed with black map: {}, white map: {}", fade_param->black_map, fade_param->white_map);
        context.set_request_status(epoc::error_none);
    }
    
    void window_user::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        epoc::version cli_ver = client->client_version();

        // Patching out user opcode.
        if (cli_ver.major == 1 && cli_ver.minor == 0) {
            if (cli_ver.build <= 171) {
                if (cmd.header.op >= EWsWinOpSendAdvancedPointerEvent) {
                    // Send advanced pointer event opcode does not exist in the version.
                    cmd.header.op += 1;
                }
            }
        }

        bool result = execute_command_for_general_node(ctx, cmd);

        if (result) {
            return;
        }

        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);
        //LOG_TRACE("Window user op: {}", (int)op);

        switch (op) {
        case EWsWinOpRequiredDisplayMode: {
            // On s60 and fowards, this method is ignored. So even with lower version, just ignore
            // them. Like they don't mean anything.
            LOG_TRACE("SetRequiredDisplayMode ignored.");
            [[fallthrough]];
        }

        // Fall through to get system display mode
        case EWsWinOpGetDisplayMode: {
            ctx.write_arg_pkg<epoc::display_mode>(reply_slot, scr->disp_mode);
            ctx.set_request_status(epoc::error_none);

            break;
        }
        
        case EWsWinOpSetExtent: {
            ws_cmd_set_extent *extent = reinterpret_cast<decltype(extent)>(cmd.data_ptr);
            set_extent(extent->pos, extent->size);
            ctx.set_request_status(epoc::error_none);
            break;
        }

        case EWsWinOpSetPos: {
            eka2l1::vec2 *pos_to_set = reinterpret_cast<eka2l1::vec2*>(cmd.data_ptr);
            pos = *pos_to_set;
            ctx.set_request_status(epoc::error_none);
            break;
        }

        // Get window size.
        case EWsWinOpSize: {
            ctx.write_arg_pkg<eka2l1::vec2>(reply_slot, size);
            ctx.set_request_status(epoc::error_none);
            break;
        }

        case EWsWinOpSetVisible: {
            const bool op = *reinterpret_cast<bool *>(cmd.data_ptr);

            set_visible(op);
            ctx.set_request_status(epoc::error_none);

            break;
        }

        case EWsWinOpSetNonFading: {
            set_non_fading(ctx, cmd);
            break;
        }

        case EWsWinOpSetShadowHeight: {
            shadow_height = *reinterpret_cast<int *>(cmd.data_ptr);
            ctx.set_request_status(epoc::error_none);

            break;
        }

        case EWsWinOpShadowDisabled: {
            flags &= ~flags_shadow_disable;

            if (*reinterpret_cast<bool *>(cmd.data_ptr)) {
                flags |= flags_shadow_disable;
            }

            ctx.set_request_status(epoc::error_none);

            break;
        }

        case EWsWinOpSetBackgroundColor: {
            if (cmd.header.cmd_len == 0) {
                clear_color = -1;
                ctx.set_request_status(epoc::error_none);

                break;
            }

            clear_color = *reinterpret_cast<int *>(cmd.data_ptr);
            ctx.set_request_status(epoc::error_none);
            break;
        }

        case EWsWinOpPointerFilter: {
            LOG_TRACE("Filtering pointer event type");

            ws_cmd_pointer_filter *filter_info = reinterpret_cast<ws_cmd_pointer_filter *>(cmd.data_ptr);
            filter &= ~filter_info->mask;
            filter |= filter_info->flags;

            ctx.set_request_status(epoc::error_none);
            break;
        }

        case EWsWinOpSetPointerGrab: {
            flags &= ~flags_allow_pointer_grab;

            if (*reinterpret_cast<bool *>(cmd.data_ptr)) {
                flags |= flags_allow_pointer_grab;
            }

            ctx.set_request_status(epoc::error_none);
            break;
        }

        case EWsWinOpActivate: {
            flags |= flags_active;

            // When a window actives, a redraw is needed
            // Redraw happens with all of the screen
            // NOTE: This causes error. Let's not touch them.
            //[[fallthrough]];
            ctx.set_request_status(epoc::error_none);
            break;
        }

        case EWsWinOpInvalidateFull: {
            irect.top = pos;
            irect.size = size;

            // Invalidate the whole window
            client->queue_redraw(this, rect(pos, pos + size));
            client->trigger_redraw();

            ctx.set_request_status(epoc::error_none);

            break;
        }

        case EWsWinOpInvalidate: {
            struct invalidate_rect {
                eka2l1::vec2 in_top_left;
                eka2l1::vec2 in_bottom_right;
            };

            const invalidate_rect prototype_irect = *reinterpret_cast<invalidate_rect *>(cmd.data_ptr);
            irect.top = prototype_irect.in_top_left;
            irect.size = prototype_irect.in_bottom_right - prototype_irect.in_top_left;

            // Invalidate needs redraw
            redraw_evt_id = client->queue_redraw(this, rect(irect.top, irect.size));
            client->trigger_redraw();

            ctx.set_request_status(epoc::error_none);

            break;
        }

        case EWsWinOpBeginRedraw: case EWsWinOpBeginRedrawFull: {
            begin_redraw(ctx, cmd);
            break;
        }

        case EWsWinOpEndRedraw: {
            end_redraw(ctx, cmd);
            break;
        }

        case EWsWinOpSetSize: case EWsWinOpSetSizeErr: {
            set_size(ctx, cmd);
            break;
        }

        case EWsWinOpSetFade:
            set_fade(ctx, cmd);
            break;

        default: {
            LOG_ERROR("Unimplemented window user opcode 0x{:X}!", cmd.header.op);
            break;
        }
        }
    }
}
