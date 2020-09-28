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

#include <services/window/classes/gctx.h>
#include <services/window/classes/scrdvc.h>
#include <services/window/classes/wingroup.h>
#include <services/window/classes/winuser.h>
#include <services/window/op.h>
#include <services/window/opheader.h>
#include <services/window/screen.h>
#include <services/window/window.h>

#include <kernel/timing.h>

#include <common/log.h>
#include <common/vecx.h>

#include <utils/err.h>

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

    eka2l1::vec2 window_top_user::absolute_position() const {
        return { 0, 0 };
    }

    window_user::window_user(window_server_client_ptr client, screen *scr, window *parent, const epoc::window_type type_of_window, const epoc::display_mode dmode, const std::uint32_t client_handle)
        : window_user_base(client, scr, parent, window_kind::client)
        , win_type(type_of_window)
        , pos(0, 0)
        , size(0, 0)
        , resize_needed(false)
        , clear_color(0xFFFFFFFF)
        , filter(pointer_filter_type::pointer_enter | pointer_filter_type::pointer_drag | pointer_filter_type::pointer_move)
        , cursor_pos(-1, -1)
        , dmode(dmode)
        , driver_win_id(0)
        , shadow_height(0)
        , max_pointer_buffer_(0)
        , last_draw_(0) {
        if (parent->type != epoc::window_kind::top_client && parent->type != epoc::window_kind::client) {
            LOG_ERROR("Parent is not a window client type!");
        } else {
            if (parent->type == epoc::window_kind::client) {
                // Inherits parent's size
                epoc::window_user *user = reinterpret_cast<epoc::window_user *>(parent);
                size = user->size;
            } else {
                // Going fullscreen
                size = scr->size();
            }
        }

        set_client_handle(client_handle);
    }

    window_user::~window_user() {
        client->remove_redraws(this);
    }

    eka2l1::vec2 window_user::get_origin() {
        return pos;
    }

    eka2l1::rect window_user::bounding_rect() const {
        eka2l1::rect bound;
        bound.top = { 0, 0 };
        bound.size = size;

        return bound;
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

        if (size != new_size) {
            size = new_size;
            redraw_region.make_empty();

            resize_needed = true;

            // We need to invalidate the whole new window
            invalidate(bounding_rect());
        }
    }

    static bool should_purge_window_user(void *win, epoc::event &evt) {
        epoc::window_user *user = reinterpret_cast<epoc::window_user *>(win);
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
            client->remove_redraws(this);
        }

        if (should_trigger_redraw) {
            // Redraw the screen. NOW!
            client->get_ws().get_anim_scheduler()->schedule(client->get_ws().get_graphics_driver(),
                scr, client->get_ws().get_ntimer()->microseconds());
        }
    }

    std::uint32_t window_user::redraw_priority(int *child_shift) {
        int ordpos = ordinal_position(false);
        if (ordpos > max_ordpos_pri) {
            ordpos = max_ordpos_pri;
        }

        int shift = 0;
        int parent_pri = reinterpret_cast<window_user_base *>(parent)->redraw_priority(&shift);

        if (shift > 0) {
            shift--;
        }

        if (child_shift) {
            *child_shift = shift;
        }

        return (parent_pri + (ordpos << (bits_per_ordpos * shift)));
    }

    eka2l1::vec2 window_user::absolute_position() const {
        return reinterpret_cast<window_user_base *>(parent)->absolute_position() + pos;
    }

    epoc::display_mode window_user::display_mode() const {
        // Fallback to screen
        return scr->disp_mode;
    }

    void window_user::take_action_on_change(kernel::thread *drawer) {
        // Want to trigger a screen redraw
        if (is_visible()) {
            epoc::animation_scheduler *sched = client->get_ws().get_anim_scheduler();
            ntimer *timing = client->get_ws().get_ntimer();
            
            // Limit the framerate, independent from screen vsync
            const std::uint64_t crr = timing->microseconds();
            const std::uint64_t time_spend_per_frame_us = 1000000 / scr->refresh_rate;
            std::uint64_t wait_time = 0;

            if (crr - last_draw_ < time_spend_per_frame_us) {
                wait_time = time_spend_per_frame_us - (crr - last_draw_);
            } else {
                wait_time = 0;
            }

            last_draw_ = crr + wait_time;
            
            sched->schedule(client->get_ws().get_graphics_driver(), scr, crr + wait_time);
            drawer->sleep(static_cast<std::uint32_t>(wait_time));
        }
    }

    void window_user::invalidate(const eka2l1::rect &irect) {
        if (irect.empty()) {
            return;
        }

        if (win_type != window_type::redraw) {
            return;
        }

        eka2l1::rect to_queue = irect;

        // Queue invalidate even if there's no change to the invalidated region.
        redraw_region.add_rect(to_queue);

        if (is_visible()) {
            client->queue_redraw(this, to_queue);
            client->trigger_redraw();
        }
    }

    void window_user::end_redraw(service::ipc_context &ctx, ws_cmd &cmd) {
        drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();

        redraw_region.eliminate(redraw_rect_curr);
        redraw_rect_curr.make_empty();

        if (resize_needed) {
            // Queue a resize command
            auto cmd_list = drv->new_command_list();
            auto cmd_builder = drv->new_command_builder(cmd_list.get());

            if (driver_win_id == 0) {
                driver_win_id = drivers::create_bitmap(drv, size);
            } else {
                cmd_builder->bind_bitmap(driver_win_id);
                cmd_builder->resize_bitmap(driver_win_id, size);
            }

            drv->submit_command_list(*cmd_list);

            resize_needed = false;
        }

        common::double_linked_queue_element *ite = attached_contexts.first();
        common::double_linked_queue_element *end = attached_contexts.end();

        bool any_flush_performed = false;

        // Set all contexts to be in recording
        do {
            if (!ite) {
                break;
            }

            epoc::graphic_context *ctx = E_LOFF(ite, epoc::graphic_context, context_attach_link);

            // Flush the queue one time.
            ctx->flush_queue_to_driver();

            if (ctx->recording) {
                // Still in active state?
                ctx->cmd_builder->bind_bitmap(driver_win_id);
                ctx->flushed = true;
            }

            ite = ite->next;

            any_flush_performed = true;
        } while (ite != end);

        if (any_flush_performed) {
            take_action_on_change(ctx.msg->own_thr);
        }

        flags &= ~flags_in_redraw;

        // Some rectangles are still not validated. Notify client about them!
        for (std::size_t i = 0; i < redraw_region.rects_.size(); i++) {
            client->queue_redraw(this, redraw_region.rects_[i]);
        }

        client->trigger_redraw();

        // LOG_DEBUG("End redraw to window 0x{:X}!", id);
        ctx.complete(epoc::error_none);
    }

    void window_user::begin_redraw(service::ipc_context &ctx, ws_cmd &cmd) {
        // LOG_TRACE("Begin redraw to window 0x{:X}!", id);
        if (flags & flags_in_redraw) {
            ctx.complete(epoc::error_in_use);
            return;
        }

        if (cmd.header.cmd_len == 0) {
            redraw_rect_curr = bounding_rect();
        } else {
            redraw_rect_curr = *reinterpret_cast<eka2l1::rect*>(cmd.data_ptr);
            redraw_rect_curr.transform_from_symbian_rectangle();
        }

        // remove all pending redraws. End redraw will report invalidates later
        client->remove_redraws(this);
        flags |= flags_in_redraw;

        ctx.complete(epoc::error_none);
    }

    void window_user::set_non_fading(service::ipc_context &context, ws_cmd &cmd) {
        const bool non_fading = *reinterpret_cast<bool *>(cmd.data_ptr);

        if (non_fading) {
            flags |= flags_non_fading;
        } else {
            flags &= ~(flags_non_fading);
        }

        context.complete(epoc::error_none);
    }

    void window_user::set_size(service::ipc_context &context, ws_cmd &cmd) {
        // refer to window_user::set_extent()
        const object_size new_size = *reinterpret_cast<object_size *>(cmd.data_ptr);
        set_extent(pos, new_size);
        context.complete(epoc::error_none);
    }

    bool window_user::clear_redraw_store() {
        return true;
    }

    void window_user::set_transparency_alpha_channel(service::ipc_context &context, ws_cmd &cmd) {
        flags |= flags_enable_alpha;
        context.complete(epoc::error_none);
    }

    void window_user::free(service::ipc_context &context, ws_cmd &cmd) {
        // Try to redraw the screen
        set_visible(false);
        remove_from_sibling_list();

        // Remove driver bitmap
        if (driver_win_id) {
            drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();

            // Queue a resize command
            auto cmd_list = drv->new_command_list();
            auto cmd_builder = drv->new_command_builder(cmd_list.get());

            cmd_builder->destroy_bitmap(driver_win_id);
            drv->submit_command_list(*cmd_list);

            driver_win_id = 0;
        }

        context.complete(epoc::error_none);

        client->delete_object(cmd.obj_handle);
    }

    void window_user::store_draw_commands(service::ipc_context &ctx, ws_cmd &cmd) {
        LOG_TRACE("Store draw command stubbed");
        ctx.complete(epoc::error_none);
    }

    void window_user::alloc_pointer_buffer(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_alloc_pointer_buffer *alloc_params = reinterpret_cast<ws_cmd_alloc_pointer_buffer *>(cmd.data_ptr);

        if ((alloc_params->max_points >= 100) || (alloc_params->max_points == 0)) {
            LOG_ERROR("Suspicious alloc pointer buffer max points detected ({})", alloc_params->max_points);
            context.complete(epoc::error_argument);

            return;
        }

        // Resize the buffers
        max_pointer_buffer_ = alloc_params->max_points;
        context.complete(epoc::error_none);
    }

    void window_user::invalidate(service::ipc_context &context, ws_cmd &cmd) {
        eka2l1::rect prototype_irect;
        eka2l1::rect whole_win = bounding_rect();

        if (cmd.header.op == EWsWinOpInvalidate) {
            prototype_irect = *reinterpret_cast<eka2l1::rect *>(cmd.data_ptr);
            prototype_irect.transform_from_symbian_rectangle();
        } else {
            // The whole window
            prototype_irect = whole_win;
        }

        if (!prototype_irect.empty() && whole_win.contains(prototype_irect)) {
            invalidate(prototype_irect);
        }
        
        context.complete(epoc::error_none);
    }

    void window_user::activate(service::ipc_context &context, ws_cmd &cmd) {
        flags |= flags_active;

        invalidate(bounding_rect());
        context.complete(epoc::error_none);
    }
    
    void window_user::get_invalid_region_count(service::ipc_context &context, ws_cmd &cmd) {
        context.complete(static_cast<std::int32_t>(redraw_region.rects_.size()));
    }

    void window_user::get_invalid_region(service::ipc_context &context, ws_cmd &cmd) {
        std::int32_t to_get_count = *reinterpret_cast<std::int32_t*>(cmd.data_ptr);

        if (to_get_count < 0) {
            context.complete(epoc::error_argument);
            return;
        }

        to_get_count = common::min<std::int32_t>(to_get_count, static_cast<std::int32_t>(redraw_region.rects_.size()));

        std::vector<eka2l1::rect> transformed;
        transformed.resize(to_get_count);

        for (std::size_t i = 0; i < to_get_count; i++) {
            transformed[i] = redraw_region.rects_[i];
            transformed[i].transform_to_symbian_rectangle();
        }

        context.write_data_to_descriptor_argument(reply_slot, reinterpret_cast<std::uint8_t*>(transformed.data()),
            to_get_count * sizeof(eka2l1::rect));
        context.complete(epoc::error_none);
    }
    
    void window_user::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        bool result = execute_command_for_general_node(ctx, cmd);
        //LOG_TRACE("Window user op: {}", (int)cmd.header.op);

        if (result) {
            return;
        }

        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsWinOpRequiredDisplayMode: {
            // On s60 and fowards, this method is ignored. So even with lower version, just ignore
            // them. Like they don't mean anything.
            LOG_TRACE("SetRequiredDisplayMode ignored.");
            [[fallthrough]];
        }

        // Fall through to get system display mode
        case EWsWinOpGetDisplayMode: {
            ctx.complete(static_cast<int>(display_mode()));
            break;
        }

        case EWsWinOpSetExtent: {
            ws_cmd_set_extent *extent = reinterpret_cast<decltype(extent)>(cmd.data_ptr);
            set_extent(extent->pos, extent->size);
            ctx.complete(epoc::error_none);
            break;
        }

        case EWsWinOpSetPos: {
            eka2l1::vec2 *pos_to_set = reinterpret_cast<eka2l1::vec2 *>(cmd.data_ptr);
            pos = *pos_to_set;
            ctx.complete(epoc::error_none);
            break;
        }

        // Get window size.
        case EWsWinOpSize: {
            ctx.write_data_to_descriptor_argument<eka2l1::vec2>(reply_slot, size);
            ctx.complete(epoc::error_none);
            break;
        }

        case EWsWinOpSetVisible: {
            const bool op = *reinterpret_cast<bool *>(cmd.data_ptr);

            set_visible(op);
            ctx.complete(epoc::error_none);

            break;
        }

        case EWsWinOpSetNonFading: {
            set_non_fading(ctx, cmd);
            break;
        }

        case EWsWinOpSetShadowHeight: {
            shadow_height = *reinterpret_cast<int *>(cmd.data_ptr);
            ctx.complete(epoc::error_none);

            break;
        }

        case EWsWinOpShadowDisabled: {
            flags &= ~flags_shadow_disable;

            if (*reinterpret_cast<bool *>(cmd.data_ptr)) {
                flags |= flags_shadow_disable;
            }

            ctx.complete(epoc::error_none);

            break;
        }

        case EWsWinOpSetBackgroundColor: {
            if (cmd.header.cmd_len == 0) {
                clear_color = -1;
                ctx.complete(epoc::error_none);

                break;
            }

            clear_color = *reinterpret_cast<int *>(cmd.data_ptr);
            ctx.complete(epoc::error_none);
            break;
        }

        case EWsWinOpPointerFilter: {
            ws_cmd_pointer_filter *filter_info = reinterpret_cast<ws_cmd_pointer_filter *>(cmd.data_ptr);
            filter &= ~filter_info->mask;
            filter |= filter_info->flags;

            ctx.complete(epoc::error_none);
            break;
        }

        case EWsWinOpSetPointerGrab: {
            flags &= ~flags_allow_pointer_grab;

            if (*reinterpret_cast<bool *>(cmd.data_ptr)) {
                flags |= flags_allow_pointer_grab;
            }

            ctx.complete(epoc::error_none);
            break;
        }

        case EWsWinOpActivate:
            activate(ctx, cmd);
            break;

        case EWsWinOpInvalidateFull:
        case EWsWinOpInvalidate:
            invalidate(ctx, cmd);

        case EWsWinOpBeginRedraw:
        case EWsWinOpBeginRedrawFull: {
            begin_redraw(ctx, cmd);
            break;
        }

        case EWsWinOpEndRedraw: {
            end_redraw(ctx, cmd);
            break;
        }

        case EWsWinOpSetSize:
        case EWsWinOpSetSizeErr: {
            set_size(ctx, cmd);
            break;
        }

        case EWsWinOpSetFade:
            set_fade(ctx, cmd);
            break;

        case EWsWinOpGetIsFaded:
            ctx.complete(static_cast<bool>(flags & flags_faded));
            break;

        case EWsWinOpSetTransparencyAlphaChannel:
            set_transparency_alpha_channel(ctx, cmd);
            break;

        case EWsWinOpFree:
            free(ctx, cmd);
            break;

        case EWsWinOpWindowGroupId:
            ctx.complete(get_group()->id);
            break;

        case EWsWinOpPosition:
            ctx.write_data_to_descriptor_argument<eka2l1::vec2>(reply_slot, pos);
            ctx.complete(epoc::error_none);
            break;

        case EWsWinOpAbsPosition:
            ctx.write_data_to_descriptor_argument<eka2l1::vec2>(reply_slot, absolute_position());
            ctx.complete(epoc::error_none);
            break;

        case EWsWinOpGetInvalidRegionCount:
            get_invalid_region_count(ctx, cmd);
            break;

        case EWsWinOpGetInvalidRegion:
            get_invalid_region(ctx, cmd);
            break;

        case EWsWinOpSetShape:
            LOG_WARN("SetShape stubbed");

            ctx.complete(epoc::error_none);
            break;

        case EWsWinOpSetCornerType:
            LOG_WARN("SetCornerType stubbed");

            ctx.complete(epoc::error_none);
            break;

        case EWsWinOpSetColor:
            LOG_WARN("SetColor stubbed");

            ctx.complete(epoc::error_none);
            break;

        case EWsWinOpStoreDrawCommands:
            store_draw_commands(ctx, cmd);
            break;

        case EWsWinOpAllocPointerMoveBuffer:
            alloc_pointer_buffer(ctx, cmd);
            break;

        default: {
            LOG_ERROR("Unimplemented window user opcode 0x{:X}!", cmd.header.op);
            break;
        }
        }
    }
}
