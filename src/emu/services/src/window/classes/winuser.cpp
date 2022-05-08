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

#include <services/window/classes/dsa.h>
#include <services/window/classes/gctx.h>
#include <services/window/classes/scrdvc.h>
#include <services/window/classes/wingroup.h>
#include <services/window/classes/winuser.h>
#include <services/window/op.h>
#include <services/window/opheader.h>
#include <services/window/screen.h>
#include <services/window/util.h>
#include <services/window/window.h>
#include <services/fbs/fbs.h>

#include <kernel/kernel.h>
#include <kernel/timing.h>

#include <common/log.h>
#include <common/vecx.h>
#include <config/config.h>

#include <drivers/itc.h>

#include <utils/err.h>

namespace eka2l1::epoc {
    static constexpr std::uint8_t bits_per_ordpos = 4;
    static constexpr std::uint8_t max_ordpos_pri = 0b1111;
    static constexpr std::uint8_t max_pri_level = (sizeof(std::uint32_t) / bits_per_ordpos) - 1;

    // ======================= WINDOW TOP USER (CLIENT) ===============================
    top_canvas::top_canvas(window_server_client_ptr client, screen *scr, window *parent)
        : canvas_interface(client, scr, parent, window_kind::top_client) {
    }

    std::uint32_t top_canvas::redraw_priority(int *shift) {
        const std::uint32_t ordinal_pos = std::min<std::uint32_t>(max_ordpos_pri, ordinal_position(false));
        if (shift) {
            *shift = max_pri_level;
        }

        return (ordinal_pos << (max_pri_level * bits_per_ordpos));
    }

    eka2l1::vec2 top_canvas::get_origin() {
        return { 0, 0 };
    }

    eka2l1::vec2 top_canvas::absolute_position() const {
        return { 0, 0 };
    }

    canvas_base::canvas_base(window_server_client_ptr client, screen *scr, window *parent, const epoc::window_type type_of_window, const epoc::display_mode dmode, const std::uint32_t client_handle)
        : canvas_interface(client, scr, parent, window_kind::client)
        , win_type(type_of_window)
        , pos(0, 0)
        , resize_needed(false)
        , clear_color_enable(true)
        , clear_color(0xFFFFFFFF)
        , filter(pointer_filter_type::pointer_enter | pointer_filter_type::pointer_drag | pointer_filter_type::pointer_move)
        , cursor_pos(-1, -1)
        , dmode(dmode)
        , shadow_height(0)
        , max_pointer_buffer_(0)
        , last_draw_(0)
        , last_fps_sync_(0)
        , fps_count_(0) {
        set_initial_state();

        abs_rect.top = reinterpret_cast<canvas_interface *>(parent)->absolute_position();
        abs_rect.size = eka2l1::vec2(0, 0);

        if (parent->type != epoc::window_kind::top_client && parent->type != epoc::window_kind::client) {
            LOG_ERROR(SERVICE_WINDOW, "Parent is not a window client type!");
        } else {
            if (parent->type == epoc::window_kind::client) {
                // Inherits parent's size
                epoc::canvas_base *user = reinterpret_cast<epoc::canvas_base *>(parent);
                abs_rect.size = user->abs_rect.size;
            } else {
                // Going fullscreen
                abs_rect.size = scr->size();
            }
        }
        
        // LOG_TRACE(SERVICE_WINDOW, "Window {} created with size {}x{} and parent {}", id, abs_rect.size.x, abs_rect.size.y, parent->id);

        kernel_system *kern = client->get_ws().get_kernel_system();
        if (kern->get_epoc_version() >= epocver::epoc94) {
            flags |= flag_winmode_fixed;
        }

        set_client_handle(client_handle);
        scr->need_update_visible_regions(true);
    }

    canvas_base::~canvas_base() {
        set_visible(false);

        remove_from_sibling_list();

        if (scr) {
            scr->need_update_visible_regions(true);
        }

        client->remove_redraws(this);
    }

    bool canvas_base::is_dsa_active() const {
        return !directs_.empty();
    }

    void canvas_base::add_dsa_active(dsa *dsa) {
        auto result = std::find(directs_.begin(), directs_.end(), dsa);
        if (result == directs_.end()) {
            directs_.push_back(dsa);
        }
    }

    void canvas_base::remove_dsa_active(dsa *dsa) {
        auto result = std::find(directs_.begin(), directs_.end(), dsa);
        if (result != directs_.end()) {
            directs_.erase(result);
        }
    }

    void canvas_base::add_draw_command(gdi_store_command &command) {
        if (win_type == window_type::blank) {
            return;
        }

        if (!pending_segment_) {
            pending_segment_ = std::make_unique<gdi_store_command_segment>();
        }

        if (command.opcode_ == gdi_store_command_draw_bitmap) {
            // We must do it sync now...
            gdi_store_command_draw_bitmap_data &data = command.get_data_struct<gdi_store_command_draw_bitmap_data>();
            gdi_store_command new_update_command_main;
            gdi_store_command new_update_command_mask;

            epoc::bitmap_cache *bcache = client->get_ws().get_bitmap_cache();
            drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();

            epoc::bitwise_bitmap *source_bitmap_bw = reinterpret_cast<epoc::bitwise_bitmap*>(data.main_fbs_bitmap_);
            epoc::bitwise_bitmap *mask_bitmap_bw = reinterpret_cast<epoc::bitwise_bitmap*>(data.mask_fbs_bitmap_);

            if ((data.gdi_flags_ & GDI_STORE_COMMAND_MAIN_RAW) == 0) {
                source_bitmap_bw = reinterpret_cast<fbsbitmap*>(data.main_fbs_bitmap_)->final_clean()->bitmap_;
            }

            data.main_drv_ = bcache->add_or_get(drv, source_bitmap_bw, nullptr, &new_update_command_main);
            
            if (mask_bitmap_bw) {
                if ((data.gdi_flags_ & GDI_STORE_COMMAND_MASK_RAW) == 0) {
                    mask_bitmap_bw = reinterpret_cast<fbsbitmap*>(data.mask_fbs_bitmap_)->final_clean()->bitmap_;
                }

                data.mask_drv_ = bcache->add_or_get(drv, mask_bitmap_bw, nullptr, &new_update_command_mask);
            }

            if (new_update_command_main.opcode_ != gdi_store_command_invalid) {
                pending_segment_->add_command(new_update_command_main);
            }

            if (new_update_command_mask.opcode_ != gdi_store_command_invalid) {
                pending_segment_->add_command(new_update_command_mask);
            }
        }

        pending_segment_->add_command(command);
    }

    bool canvas_base::can_be_physically_seen() const {
        return is_visible() && !visible_region.empty();
    }

    bool canvas_base::is_visible() const {
        return ((flags & flags_active) && (flags & flags_visible));
    }

    eka2l1::vec2 canvas_base::get_origin() {
        return pos;
    }

    eka2l1::rect canvas_base::bounding_rect() const {
        eka2l1::rect bound;
        bound.top = { 0, 0 };
        bound.size = abs_rect.size;

        return bound;
    }

    eka2l1::rect canvas_base::absolute_rect() const {
        return abs_rect;
    }

    eka2l1::vec2 canvas_base::size() const {
        return abs_rect.size;
    }

    eka2l1::vec2 canvas_base::size_for_egl_surface() const {
        if ((flags & epoc::window::flag_fix_native_orientation) && (abs_rect.size == scr->current_mode().size)) {
            // Fullscreen window, but must change the size to native orientation
            return scr->size();
        }

        return abs_rect.size;
    }

    void canvas_base::report_visiblity_change() {
        if ((flags & flag_visiblity_event_report) == 0) {
            return;
        }

        if ((flags & flags_active) == 0) {
            return;
        }

        epoc::event visi_change_evt;
        visi_change_evt.type = epoc::event_code::window_visibility_change;
        visi_change_evt.time = client->get_ws().get_kernel_system()->home_time();
        visi_change_evt.handle = client_handle;
        visi_change_evt.win_visibility_change_evt_.flags_ = 0;

        if (visible_region.empty()) {
            visi_change_evt.win_visibility_change_evt_.flags_ = epoc::window_visiblity_changed_event::not_visible;
        } else {
            if (((flags & flag_shape_region) && (visible_region.identical(shape_region))) ||
                (((flags & flag_shape_region) == 0) && (visible_region.rects_.size() == 1) && (visible_region.rects_[0] == bounding_rect()))) {
                visi_change_evt.win_visibility_change_evt_.flags_ = (epoc::window_visiblity_changed_event::partially_visible | epoc::window_visiblity_changed_event::fully_visible);
            } else {
                visi_change_evt.win_visibility_change_evt_.flags_ = epoc::window_visiblity_changed_event::partially_visible;
            }
        }

        window::queue_event(visi_change_evt);
    }

    void canvas_base::queue_event(const epoc::event &evt) {
        if (!is_visible()) {
            // TODO: Im not sure... I think it can certainly receive
            LOG_TRACE(SERVICE_WINDOW, "The client window 0x{:X} is not visible, and can't receive any events", id);
            return;
        }

        window::queue_event(evt);
    }

    struct window_absolute_postion_change_walker : public window_tree_walker {
        eka2l1::vec2 diff_;

        explicit window_absolute_postion_change_walker(const eka2l1::vec2 &diff)
            : diff_(diff) {
        }

        bool do_it(epoc::window *win) override {
            if (win->type == window_kind::client) {
                canvas_base *user = reinterpret_cast<canvas_base*>(win);
                user->recalculate_absolute_position(diff_);
            }

            return false;
        }
    };

    void canvas_base::recalculate_absolute_position(const eka2l1::vec2 &diff) {
        if (diff == eka2l1::vec2(0, 0)) {
            return;
        }

        // Relative position stays the same, however the absolute one got a definite change
        abs_rect.top += diff;

        if (flags & flag_shape_region) {
            shape_region.advance(diff);
        }
    }

    void canvas_base::set_extent(const eka2l1::vec2 &top, const eka2l1::vec2 &new_size) {
        eka2l1::vec2 pos_diff = top - pos;

        const bool size_changed = (new_size != abs_rect.size);
        const bool pos_changed = (pos_diff != eka2l1::vec2(0, 0));

        handle_extent_changed(new_size, top);

        abs_rect.top += pos_diff;
        abs_rect.size = new_size;

        pos = top;

        if (pos_changed) {
            // Change the absolute position of children too!
            window_absolute_postion_change_walker walker(pos_diff);
            walk_tree(&walker, epoc::window_tree_walk_style::bonjour_children);
        }

        if (pos_changed || size_changed) {
            // Force a visible region update when the next screen redraw comes
            if (is_visible()) {
                scr->need_update_visible_regions(true);
            }
        }
    }

    static bool should_purge_canvas_base(void *win, epoc::event &evt) {
        epoc::canvas_base *user = reinterpret_cast<epoc::canvas_base *>(win);
        if (user->client_handle == evt.handle) {
            return false;
        }

        return true;
    }

    void canvas_base::set_visible(const bool vis) {
        bool should_trigger_redraw = false;
        bool current_visible_status = (flags & flags_visible) != 0;

        if (current_visible_status != vis) {
            should_trigger_redraw = true;
        } else {
            return;
        }

        flags &= ~flags_visible;

        if (vis) {
            flags |= flags_visible;
        } else {
            // Purge all queued events now that the window is not visible anymore
            client->walk_event(should_purge_canvas_base, this);
        }

        if (should_trigger_redraw) {
            scr->need_update_visible_regions(true);

            // Redraw the screen. NOW!
            client->get_ws().get_anim_scheduler()->schedule(client->get_ws().get_graphics_driver(),
                scr, client->get_ws().get_ntimer()->microseconds());
        }
    }

    std::uint32_t canvas_base::redraw_priority(int *child_shift) {
        int ordpos = ordinal_position(false);
        if (ordpos > max_ordpos_pri) {
            ordpos = max_ordpos_pri;
        }

        int shift = 0;
        int parent_pri = reinterpret_cast<canvas_interface *>(parent)->redraw_priority(&shift);

        if (shift > 0) {
            shift--;
        }

        if (child_shift) {
            *child_shift = shift;
        }

        return (parent_pri + (ordpos << (bits_per_ordpos * shift)));
    }

    epoc::window_group *canvas_base::get_group() {
        epoc::window_group *are_you = reinterpret_cast<epoc::window_group *>(parent);
        while (are_you->type != epoc::window_kind::group) {
            are_you = reinterpret_cast<epoc::window_group *>(are_you->parent);
        }

        return are_you;
    }

    eka2l1::vec2 canvas_base::absolute_position() const {
        return abs_rect.top;
    }

    epoc::display_mode canvas_base::display_mode() const {
        // Fallback to screen
        return scr->disp_mode;
    }

    std::uint64_t canvas_base::try_update(kernel::thread *drawer) {
        if (scr->need_update_visible_regions()) {
            scr->recalculate_visible_regions();
        }

        // Want to trigger a screen redraw
        if (can_be_physically_seen()) {
            epoc::animation_scheduler *sched = client->get_ws().get_anim_scheduler();
            ntimer *timing = client->get_ws().get_ntimer();

            // Limit the framerate, independent from screen vsync
            const std::uint64_t crr = timing->microseconds();
            const std::uint64_t time_spend_per_frame_us = 1000000 / scr->refresh_rate;

            std::uint64_t wait_time = 0;

            if (crr < time_spend_per_frame_us + last_draw_) {
                // Originally - (crr - last_draw_), but preventing overflow
                wait_time = time_spend_per_frame_us + last_draw_ - crr;
            } else {
                wait_time = 0;
            }

            last_draw_ = ((crr + time_spend_per_frame_us - 1) / time_spend_per_frame_us) * time_spend_per_frame_us;

            if (crr - last_fps_sync_ >= common::microsecs_per_sec) {
                scr->last_fps = fps_count_;

                last_fps_sync_ = crr;
                fps_count_ = 0;
            }

            fps_count_++;

            // We need a redraw from the client side, so set this
            scr->set_client_draw_pending();

            sched->schedule(client->get_ws().get_graphics_driver(), scr, crr + wait_time);
            
            if (drawer) {
                drawer->sleep(static_cast<std::uint32_t>(wait_time));
            }

            return wait_time;
        }

        return 0;
    }

    void canvas_base::set_non_fading(service::ipc_context &context, ws_cmd &cmd) {
        const std::uint32_t non_fading = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);

        if (non_fading) {
            flags |= flags_non_fading;
        } else {
            flags &= ~(flags_non_fading);
        }

        context.complete(epoc::error_none);
    }

    void canvas_base::set_size(service::ipc_context &context, ws_cmd &cmd) {
        // refer to canvas_base::set_extent()
        const object_size new_size = *reinterpret_cast<object_size *>(cmd.data_ptr);
        set_extent(pos, new_size);
        context.complete(epoc::error_none);
    }

    void canvas_base::set_transparency_alpha_channel(service::ipc_context &context, ws_cmd &cmd) {
        flags |= flags_enable_alpha;
        LOG_TRACE(SERVICE_WINDOW, "Enable alpha for window {}", id);
        context.complete(epoc::error_none);
    }

    void canvas_base::destroy(service::ipc_context &context, ws_cmd &cmd) {
        // Try to redraw the screen
        on_command_batch_done(context);

        context.complete(epoc::error_none);
        client->delete_object(cmd.obj_handle);
    }

    void canvas_base::alloc_pointer_buffer(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_alloc_pointer_buffer *alloc_params = reinterpret_cast<ws_cmd_alloc_pointer_buffer *>(cmd.data_ptr);

        if ((alloc_params->max_points >= 100) || (alloc_params->max_points == 0)) {
            LOG_ERROR(SERVICE_WINDOW, "Suspicious alloc pointer buffer max points detected ({})", alloc_params->max_points);
            context.complete(epoc::error_argument);

            return;
        }

        // Resize the buffers
        max_pointer_buffer_ = alloc_params->max_points;
        context.complete(epoc::error_none);
    }
    
    void canvas_base::fix_native_orientation(service::ipc_context &context, ws_cmd &cmd) {
        if (flags & flags_active) {
            LOG_TRACE(SERVICE_WINDOW, "The window has already been activated, fix native orientation is illegal!");
            context.complete(epoc::error_none);

            return;
        }

        if (flags & flag_fix_native_orientation) {
            LOG_TRACE(SERVICE_WINDOW, "Fix native orientation has already been called before, no need to recall!");
            context.complete(epoc::error_none);

            return;
        }

        flags |= flag_fix_native_orientation;
        context.complete(epoc::error_none);
    }

    bool redraw_msg_canvas::clear_redraw_store() {
        //has_redraw_content(false);
        return true;
    }

    void redraw_msg_canvas::store_draw_commands(service::ipc_context &ctx, ws_cmd &cmd) {
        LOG_TRACE(SERVICE_WINDOW, "Store draw command stubbed");
        ctx.complete(epoc::error_none);
    }

    void redraw_msg_canvas::invalidate(service::ipc_context &context, ws_cmd &cmd) {
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

            if (scr->scr_config.flicker_free) {
                background_region.add_rect(prototype_irect);
            }
        }

        context.complete(epoc::error_none);
    }

    void redraw_msg_canvas::on_activate() {
        invalidate(bounding_rect());
    }

    void canvas_base::activate(service::ipc_context &context, ws_cmd &cmd) {
        flags |= flags_active;
        on_activate();

        if (is_visible()) {
            scr->need_update_visible_regions(true);
        }

        context.complete(epoc::error_none);
    }

    void canvas_base::scroll(service::ipc_context &context, ws_cmd &cmd) {
        eka2l1::rect clip_rect;
        eka2l1::point offset;
        eka2l1::rect source_rect;

        ws_cmd_scroll *scroll_data = reinterpret_cast<ws_cmd_scroll*>(cmd.data_ptr);
        offset = scroll_data->offset;

        if ((cmd.header.op == EWsWinOpScrollClip) || (cmd.header.op == EWsWinOpScrollClipRect)) {
            clip_rect = scroll_data->clip_rect;
            clip_rect.transform_from_symbian_rectangle();
        }

        if ((cmd.header.op == EWsWinOpScrollRect) || (cmd.header.op == EWsWinOpScrollClipRect)) {
            source_rect = scroll_data->source_rect;
            source_rect.transform_from_symbian_rectangle();
        }

        if (scroll(clip_rect, offset, source_rect)) {
            try_update(context.msg->own_thr);
        }

        context.complete(epoc::error_none);
    }

    void canvas_base::set_shape(service::ipc_context &context, ws_cmd &cmd) {
        std::optional<common::region> shape = get_region_from_context(context, cmd);
        if (!shape.has_value()) {
            context.complete(epoc::error_argument);
            return;
        }

        flags |= flag_shape_region;

        shape_region = std::move(shape.value());
        shape_region.advance(abs_rect.top);

        scr->recalculate_visible_regions();
        context.complete(epoc::error_none);
    }
    
    void canvas_base::enable_visiblity_change_events(service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        flags |= flag_visiblity_event_report;
        report_visiblity_change();
        
        ctx.complete(epoc::error_none);
    }

    bool canvas_base::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        bool useless = false;
        return execute_command_detail(ctx, cmd, useless);
    }

    bool canvas_base::execute_command_detail(service::ipc_context &ctx, ws_cmd &cmd, bool &did_it) {
        bool result = execute_command_for_general_node(ctx, cmd);
        bool quit = false;
        //LOG_TRACE(SERVICE_WINDOW, "Window user op: {}", (int)cmd.header.op);

        did_it = true;

        if (result) {
            return false;
        }

        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsWinOpRequiredDisplayMode: {
            // On s60v5 and fowards, this method is ignored (it seems so on s60v5, def on s^3)
            if (!(flags & flag_winmode_fixed)) {
                dmode = *reinterpret_cast<epoc::display_mode *>(cmd.data_ptr);
                if (epoc::get_num_colors_from_display_mode(dmode) > epoc::get_num_colors_from_display_mode(scr->disp_mode)) {
                    dmode = scr->disp_mode;
                }
            }
            ctx.complete(static_cast<int>(dmode));
            break;
        }

        // Fall through to get system display mode
        case EWsWinOpGetDisplayMode: {
            ctx.complete(static_cast<int>(dmode));
            break;
        }

        case EWsWinOpSetExtent:
        case EWsWinOpSetExtentErr: {
            ws_cmd_set_extent *extent = reinterpret_cast<decltype(extent)>(cmd.data_ptr);
            set_extent(extent->pos, extent->size);
            ctx.complete(epoc::error_none);
            break;
        }

        case EWsWinOpSetPos: {
            eka2l1::vec2 *pos_to_set = reinterpret_cast<eka2l1::vec2 *>(cmd.data_ptr);
            set_extent(*pos_to_set, size());
            ctx.complete(epoc::error_none);
            break;
        }

        // Get window size.
        case EWsWinOpSize: {
            ctx.write_data_to_descriptor_argument<eka2l1::vec2>(reply_slot, abs_rect.size);
            ctx.complete(epoc::error_none);
            break;
        }

        case EWsWinOpSetVisible: {
            const std::uint32_t visible = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);

            set_visible(visible != 0);
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

        case EWsWinOpSetBackgroundColor:
        case EWsWinOpSetNoBackgroundColor: {
            if ((cmd.header.cmd_len == 0) && (cmd.header.op == EWsWinOpSetNoBackgroundColor)) {
                LOG_TRACE(SERVICE_WINDOW, "Set no clear for {}", id);
                clear_color_enable = false;
                ctx.complete(epoc::error_none);

                break;
            }

            clear_color = *reinterpret_cast<int *>(cmd.data_ptr);
            clear_color_enable = true;

            kernel_system *kern = client->get_ws().get_kernel_system();

            if (!kern->is_eka1()) {
                // From EKA2, color that passed through the server is 0xaarrggbb. R and B channels are swapped
                // The call that makes the color is TRgb::Internal()
                clear_color = (clear_color & 0xFF00FF00) | ((clear_color & 0xFF) << 16) | ((clear_color & 0xFF0000) >> 16);
            }

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
            destroy(ctx, cmd);
            quit = true;
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

        case EWsWinOpSetShape:
            set_shape(ctx, cmd);
            break;

        case EWsWinOpSetCornerType:
            LOG_WARN(SERVICE_WINDOW, "SetCornerType stubbed");

            ctx.complete(epoc::error_none);
            break;

        case EWsWinOpSetColor:
            LOG_WARN(SERVICE_WINDOW, "SetColor stubbed");

            ctx.complete(epoc::error_none);
            break;

        case EWsWinOpAllocPointerMoveBuffer:
            alloc_pointer_buffer(ctx, cmd);
            break;

        case EWsWinOpScroll:
        case EWsWinOpScrollRect:
        case EWsWinOpScrollClip:
        case EWsWinOpScrollClipRect:
            scroll(ctx, cmd);
            break;

        case EWsWinOpEnableVisibilityChangeEvents:
            enable_visiblity_change_events(ctx, cmd);
            break;

        case EWsWinOpFixNativeOrientation:
            fix_native_orientation(ctx, cmd);
            break;

        default: {
            did_it = false;
            break;
        }
        }

        return quit;
    }
    
    blank_canvas::blank_canvas(window_server_client_ptr client, screen *scr, window *parent,
            const epoc::display_mode dmode, const std::uint32_t client_handle)
        : canvas_base(client, scr, parent, window_type::blank, dmode, client_handle) {

    }
        
    bool blank_canvas::draw(drivers::graphics_command_builder &builder) {
        if (!clear_color_enable || !can_be_physically_seen() || scr->scr_config.blt_offscreen) {
            return false;
        }

        if ((scr->flags_ & screen::FLAG_SERVER_REDRAW_PENDING) == 0) {
            return false;
        }

        driver_builder_.reset_list();

        eka2l1::vec2 abs_pos = absolute_position();
        auto color_extracted = common::rgba_to_vec(clear_color);

        builder.set_feature(drivers::graphics_feature::blend, false);
        builder.clip_bitmap_region(visible_region, scr->display_scale_factor);

        if (display_mode() <= epoc::display_mode::color16mu) {
            color_extracted.w = 255;
        }

        builder.set_brush_color_detail(color_extracted);
        builder.draw_rectangle(eka2l1::rect(abs_pos * scr->display_scale_factor, { 0, 0 }));

        return true;
    }

    redraw_msg_canvas::redraw_msg_canvas(window_server_client_ptr client, screen *scr, window *parent,
        const epoc::display_mode dmode, const std::uint32_t client_handle)
        : canvas_base(client, scr, parent, epoc::window_type::redraw, dmode, client_handle) {
    }

    void redraw_msg_canvas::handle_extent_changed(const eka2l1::vec2 &new_size, const eka2l1::vec2 &new_pos) {
        if (new_size != abs_rect.size) {
            resize_needed = true;

            eka2l1::rect new_bounding_rect(eka2l1::vec2{ 0 , 0 }, new_size);

            // Old WSERV behaviour not using offscreen bitmap will only clear region that is
            // explicitly invalidated by the window server. This one is, but the one submitted by
            // invalidate alone can't be considered
            if ((new_size.x > abs_rect.size.x) || (new_size.y > abs_rect.size.y)) {
                if (!scr->scr_config.blt_offscreen && !scr->scr_config.flicker_free) {
                    common::region newly_expanded;
                    newly_expanded.add_rect(new_bounding_rect);
                    newly_expanded.eliminate(eka2l1::rect(eka2l1::vec2{ 0, 0 }, abs_rect.size));

                    for (std::size_t i = 0; i < newly_expanded.rects_.size(); i++) {
                        background_region.add_rect(newly_expanded.rects_[i]);
                    }
                }
    
                redraw_region.make_empty();
                invalidate(new_bounding_rect);
            } else {
                redraw_region.clip(new_bounding_rect);
            }

            if (!scr->scr_config.blt_offscreen && scr->scr_config.flicker_free) {
                // Clear the whole background
                background_region.make_empty();
                background_region.add_rect(eka2l1::rect(eka2l1::vec2{ 0, 0 }, new_size));
            }
        }
    }

    void redraw_msg_canvas::get_invalid_region_count(service::ipc_context &context, ws_cmd &cmd) {
        context.complete(static_cast<std::int32_t>(redraw_region.rects_.size()));
    }

    void redraw_msg_canvas::get_invalid_region(service::ipc_context &context, ws_cmd &cmd) {
        std::int32_t to_get_count = *reinterpret_cast<std::int32_t *>(cmd.data_ptr);

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

        context.write_data_to_descriptor_argument(reply_slot, reinterpret_cast<std::uint8_t *>(transformed.data()),
            to_get_count * sizeof(eka2l1::rect));
        context.complete(epoc::error_none);
    }

    void redraw_msg_canvas::invalidate(const eka2l1::rect &irect) {
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

    void redraw_msg_canvas::end_redraw(service::ipc_context &ctx, ws_cmd &cmd) {
        redraw_rect_curr.make_empty();
        redraw_segments_.promote_last_segment();

        if (content_changed()) {
            try_update(ctx.msg->own_thr);
        }

        flags &= ~flags_in_redraw;
        content_changed(false);

        // Some rectangles are still not validated. Notify client about them!
        for (std::size_t i = 0; i < redraw_region.rects_.size(); i++) {
            client->queue_redraw(this, redraw_region.rects_[i]);
        }

        client->trigger_redraw();

        // LOG_DEBUG(SERVICE_WINDOW, "End redraw to window 0x{:X}!", id);
        ctx.complete(epoc::error_none);
    }

    void redraw_msg_canvas::begin_redraw(service::ipc_context &ctx, ws_cmd &cmd) {
        // LOG_TRACE(SERVICE_WINDOW, "Begin redraw to window 0x{:X}!", id);
        if (flags & flags_in_redraw) {
            ctx.complete(epoc::error_in_use);
            return;
        }

        if (cmd.header.cmd_len == 0) {
            redraw_rect_curr = bounding_rect();
        } else {
            redraw_rect_curr = *reinterpret_cast<eka2l1::rect *>(cmd.data_ptr);
            redraw_rect_curr.transform_from_symbian_rectangle();
        }

        // Remove the given rect from region need to redraw
        // In case app try to get the invalid region infos inside a redraw!
        // Yes. THPS. Get invalid region then forgot to free, leak mem all over. I have pain!!
        redraw_region.eliminate(redraw_rect_curr);

        // remove all pending redraws. End redraw will report invalidates later
        client->remove_redraws(this);
        redraw_segments_.add_new_segment(redraw_rect_curr, epoc::gdi_store_command_segment_pending_redraw);

        flags |= flags_in_redraw;

        // Go to all contexts and update clipping
        common::double_linked_queue_element *ite = attached_contexts.first();
        common::double_linked_queue_element *end = attached_contexts.end();

        // Set all contexts to be in recording
        do {
            if (!ite) {
                break;
            }

            epoc::graphic_context *ctx = E_LOFF(ite, epoc::graphic_context, context_attach_link);

            if (ctx->recording) {
                // Still in active state?
                ctx->do_submit_clipping();
                ctx->flushed = true;
            }

            ite = ite->next;
        } while (ite != end);

        ctx.complete(epoc::error_none);
    }

    void redraw_msg_canvas::add_draw_command(gdi_store_command &command) {
        const std::lock_guard<std::mutex> guard(scr->screen_mutex);

        if ((flags & flags_in_redraw) == 0) {
            eka2l1::rect full_size_rect(eka2l1::vec2(0, 0), abs_rect.size);

            // Not in redraw? Try to cleanup non redraw segments to make ways
            if (redraw_segments_.clean_old_nonredraw_segments()) {
                // With no redraw store, segments aging is ineffective. So just ignore...
                if (!client->get_ws().no_redraw_storing_enabled())
                    invalidate(full_size_rect);
            }

            gdi_store_command_segment *current_segment = redraw_segments_.get_current_segment();
            if (!current_segment || (current_segment->type_ != gdi_store_command_segment_non_redraw)) {
                // Create a new non redraw segment, covers the entire screen
                redraw_segments_.add_new_segment(full_size_rect, gdi_store_command_segment_non_redraw);
            }
        }

        content_changed(true);

        gdi_store_command_segment *current_segment = redraw_segments_.get_current_segment();
        current_segment->add_command(command);

        canvas_base::add_draw_command(command);
    }

    bool redraw_msg_canvas::scroll(eka2l1::rect clip_space, const eka2l1::vec2 offset, eka2l1::rect source_rect) {
        // Don't have framebuffer, so can't copy now!
        eka2l1::rect rect = source_rect;
        rect = rect.intersect(clip_space);
        invalidate(rect);

        rect = source_rect;
        rect.top += offset;
        rect = rect.intersect(clip_space);

        invalidate(rect);

        return true;
    }

    std::uint64_t redraw_msg_canvas::try_update(kernel::thread *drawer) {
        return canvas_base::try_update(drawer);
    }

    bool redraw_msg_canvas::draw(drivers::graphics_command_builder &builder) {
        if (!can_be_physically_seen()) {
            // No need to redraw this window yet. It doesn't even have any content ready.
            return false;
        }

        // Check if extent is just invalid
        if (size().x == 0 || size().y == 0) {
            // No one can see this. Leave it for now.
            return false;
        }

        // If it does not have content drawn to it, it makes no sense to draw the background
        // Else, there's a flag in window server that enables clear on any siutation
        auto draw_background_color = [&]() {
            if (!scr->scr_config.blt_offscreen && clear_color_enable && !background_region.empty()) {        
                background_region.advance(abs_rect.top);
                background_region = background_region.intersect(visible_region);

                builder.clip_bitmap_region(background_region, scr->display_scale_factor);

                auto color_extracted = common::rgba_to_vec(clear_color);

                if (display_mode() <= epoc::display_mode::color16mu) {
                    color_extracted.w = 255;
                }

                builder.set_brush_color_detail(color_extracted);
                builder.draw_rectangle(eka2l1::rect(abs_rect.top * scr->display_scale_factor, { 0, 0 }));

                background_region.make_empty();
            } else {
                builder.set_brush_color(eka2l1::vec3(255, 255, 255));
                builder.set_feature(drivers::graphics_feature::clipping, false);
            }
        };

        builder.set_feature(drivers::graphics_feature::blend, false);

        eka2l1::drivers::filter_option filter = (client->get_ws().get_kernel_system()->get_config()->nearest_neighbor_filtering ?
            eka2l1::drivers::filter_option::nearest : eka2l1::drivers::filter_option::linear);

        if (scr->flags_ & screen::FLAG_SERVER_REDRAW_PENDING) {
            auto &segments = redraw_segments_.get_segments();

            if (!segments.empty()) {
                draw_background_color();

                builder.clip_bitmap_region(visible_region, scr->display_scale_factor);

                gdi_command_builder gdi_builder(client->get_ws().get_graphics_driver(), builder,
                    *client->get_ws().get_bitmap_cache(), filter, abs_rect.top, scr->display_scale_factor,
                    visible_region);

                for (std::size_t i = 0; i < segments.size(); i++) {
                    if (segments[i]->type_ != gdi_store_command_segment_pending_redraw) {
                        gdi_builder.build_segment(*segments[i]);
                    }
                }
            }
        }

        if (scr->flags_ & screen::FLAG_CLIENT_REDRAW_PENDING) {
            drivers::command_list cmd_list = driver_builder_.retrieve_command_list();
            if (pending_segment_ || !cmd_list.empty()) {
                draw_background_color();
            }

            if (pending_segment_) {
                builder.clip_bitmap_region(visible_region, scr->display_scale_factor);

                gdi_command_builder gdi_builder(client->get_ws().get_graphics_driver(), builder,
                    *client->get_ws().get_bitmap_cache(), filter, abs_rect.top, scr->display_scale_factor,
                    visible_region);

                gdi_builder.build_segment(*pending_segment_);
                pending_segment_.reset();
            }

            if (!cmd_list.empty()) {
                builder.clip_bitmap_region(visible_region, scr->display_scale_factor);

                if (!builder.merge(cmd_list)) {
                    // Full, need to flush the old one maybe
                    drivers::command_list screen_draw_prev_list = builder.retrieve_command_list();
                    drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();

                    drv->submit_command_list(screen_draw_prev_list);
                    builder.bind_bitmap(scr->screen_texture);

                    if (!builder.merge(cmd_list)) {
                        LOG_ERROR(SERVICE_WINDOW, "Unable to merge redraw window's command list to screen draw's command list!");
                    }
                }
            }
        }

        return true;
    }

    bool redraw_msg_canvas::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        // LOG_TRACE(SERVICE_WINDOW, "Redraw canvas opcode {}", cmd.header.op);

        bool did_it = false;
        const bool should_flush = canvas_base::execute_command_detail(ctx, cmd, did_it);

        if (did_it) {
            return should_flush;
        }

        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsWinOpInvalidateFull:
        case EWsWinOpInvalidate:
            invalidate(ctx, cmd);
            break;

        case EWsWinOpBeginRedraw:
        case EWsWinOpBeginRedrawFull: {
            begin_redraw(ctx, cmd);
            break;
        }

        case EWsWinOpEndRedraw: {
            end_redraw(ctx, cmd);
            break;
        }

        case EWsWinOpGetInvalidRegionCount:
            get_invalid_region_count(ctx, cmd);
            break;

        case EWsWinOpGetInvalidRegion:
            get_invalid_region(ctx, cmd);
            break;

        case EWsWinOpStoreDrawCommands:
            store_draw_commands(ctx, cmd);
            break;

        default:
            LOG_WARN(SERVICE_WINDOW, "Unimplemented redraw canvas opcode 0x{:X}!", cmd.header.op);
            ctx.complete(epoc::error_none);

            break;
        }

        return false;
    }
    
    bitmap_backed_canvas::bitmap_backed_canvas(window_server_client_ptr client, screen *scr, window *parent,
        const epoc::display_mode dmode, const std::uint32_t client_handle)
        : canvas_base(client, scr, parent, window_type::backed_up, dmode, client_handle)
        , bitmap_(nullptr)
        , driver_win_id(0)
        , ping_pong_driver_win_id(0) {
        resize_needed = true;
    }

    void bitmap_backed_canvas::on_activate() {
    }

    void bitmap_backed_canvas::handle_extent_changed(const eka2l1::vec2 &new_size, const eka2l1::vec2 &new_pos) {
        if (abs_rect.size != new_size) {
            if (ping_pong_driver_win_id) {
                // Let's just recreate later, just a temp for scrolling
                drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();
                drivers::graphics_command_builder builder;

                builder.destroy_bitmap(ping_pong_driver_win_id);

                drivers::command_list retrieved = builder.retrieve_command_list();
                drv->submit_command_list(retrieved);

                ping_pong_driver_win_id = 0;
            }

            if (bitmap_)
                LOG_WARN(SERVICE_WINDOW, "Backed up window size changed but FBS bitmap resize not yet supported!");
        }
    }

    bitmap_backed_canvas::~bitmap_backed_canvas() {
        if (bitmap_) {
            bitmap_->deref();
        }

        drivers::graphics_command_builder builder;
        drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();

        // Remove driver bitmap
        if (driver_win_id) {
            // Queue a resize command
            builder.destroy_bitmap(driver_win_id);
            driver_win_id = 0;
        }

        if (ping_pong_driver_win_id) {
            // Let's just recreate later, just a temp for scrolling
            drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();

            builder.destroy_bitmap(ping_pong_driver_win_id);
            ping_pong_driver_win_id = 0;
        }

        if (!builder.is_empty()) {
            drivers::command_list retrieved = builder.retrieve_command_list();
            drv->submit_command_list(retrieved);
        }
    }

    void bitmap_backed_canvas::create_backed_bitmap() {
        fbs_server *serv = client->get_ws().get_fbs_server();

        fbs_bitmap_data_info info;
        info.comp_ = epoc::bitmap_file_no_compression;
        info.data_ = nullptr;
        info.data_size_ = 0;
        info.dpm_ = display_mode();
        info.size_ = size();

        bool support_current_display_mode = true;
        bool support_dirty_bitmap = true;

        query_fbs_feature_support(serv, support_current_display_mode, support_dirty_bitmap);

        bitmap_ = serv->create_bitmap(info, true, support_current_display_mode, support_dirty_bitmap);
        if (bitmap_) {
            bitmap_->ref();
        } else {
            LOG_ERROR(SERVICE_WINDOW, "Unable to create backed up FBS bitmap for window {}", id);
        }

        if (driver_win_id) {
            drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();
            drivers::read_bitmap(drv, driver_win_id, eka2l1::point(0, 0), info.size_, get_bpp_from_display_mode(info.dpm_), bitmap_->bitmap_->data_pointer(serv));
        }
    }

    void bitmap_backed_canvas::bitmap_handle(service::ipc_context &ctx, ws_cmd &cmd) {
        if (!bitmap_) {
            create_backed_bitmap();
        }

        if (!bitmap_) {
            ctx.complete(epoc::error_no_memory);
            return;
        }

        ctx.complete(static_cast<int>(bitmap_->id));
    }

    std::uint64_t bitmap_backed_canvas::try_update(kernel::thread *drawer) {
        drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();

        if (!content_changed()) {
            return 0;
        }

        if (pending_segment_) {
            gdi_command_builder gdi_builder(client->get_ws().get_graphics_driver(), driver_builder_,
                *client->get_ws().get_bitmap_cache(), drivers::filter_option::linear, eka2l1::vec2(0, 0),
                1.0f, common::region{});

            gdi_builder.build_segment(*pending_segment_);
            pending_segment_.reset();
        }

        drivers::command_list list = driver_builder_.retrieve_command_list();
        drv->submit_command_list(list);

        driver_builder_.bind_bitmap(driver_win_id);

        // Sync back to the bitmap
        if (bitmap_) {
            if (bitmap_->bitmap_->compression_type() != epoc::bitmap_file_no_compression) {
                LOG_ERROR(SERVICE_WINDOW, "Try to sync data back to backed bitmap canvas but compression is required on the bitmap!");
            } else {
                fbs_server *serv = client->get_ws().get_fbs_server();

                bool support_current_display_mode = true;
                bool support_dirty_bitmap = true;

                query_fbs_feature_support(serv, support_current_display_mode, support_dirty_bitmap);

                eka2l1::vec2 to_sync_size(common::min<int>(bitmap_->bitmap_->header_.size_pixels.x, size().x),
                    common::min<int>(bitmap_->bitmap_->header_.size_pixels.y, size().y));

                drivers::read_bitmap(drv, driver_win_id, eka2l1::point(0, 0), to_sync_size, get_bpp_from_display_mode(
                    support_current_display_mode ? bitmap_->bitmap_->settings_.current_display_mode() : bitmap_->bitmap_->settings_.initial_display_mode()),
                    bitmap_->bitmap_->data_pointer(serv));
            }
        }

        return canvas_base::try_update(drawer);
    }

    void bitmap_backed_canvas::add_draw_command(gdi_store_command &command) {
        canvas_base::add_draw_command(command);
        content_changed(true);
    }

    void bitmap_backed_canvas::prepare_for_draw() {
        if (resize_needed) {
            drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();

            // Queue a resize command
            drivers::graphics_command_builder cmd_builder;

            if (driver_win_id == 0) {
                driver_win_id = drivers::create_bitmap(drv, abs_rect.size, 32);
            } else {
                cmd_builder.resize_bitmap(driver_win_id, abs_rect.size);
            }

            drivers::command_list retrieved = cmd_builder.retrieve_command_list();
            drv->submit_command_list(retrieved);

            resize_needed = false;
        }

        driver_builder_.bind_bitmap(driver_win_id);
    }

    void bitmap_backed_canvas::sync_from_bitmap(std::optional<common::region> reg_clip) {
        if (!bitmap_) {
            return;
        }

        prepare_for_draw();

        epoc::bitmap_cache *cache = client->get_ws().get_bitmap_cache();
        drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();

        drivers::graphics_command_builder builder;
        const drivers::handle to_draw_handle = cache->add_or_get(drv, bitmap_->bitmap_, &builder);

        eka2l1::vec2 to_draw_out_size(common::min<int>(bitmap_->bitmap_->header_.size_pixels.x, size().x),
            common::min<int>(bitmap_->bitmap_->header_.size_pixels.y, size().y));

        eka2l1::rect draw_rect({ 0, 0 }, to_draw_out_size);

        builder.bind_bitmap(driver_win_id);
        builder.set_feature(drivers::graphics_feature::clipping, false);

        if (reg_clip.has_value()) {
            builder.clip_bitmap_region(reg_clip.value(), scr->display_scale_factor);
        }

        builder.draw_bitmap(to_draw_handle, 0, draw_rect, draw_rect);

        drivers::command_list retrieved = builder.retrieve_command_list();
        drv->submit_command_list(retrieved);
    }

    void bitmap_backed_canvas::update_screen(service::ipc_context &ctx, ws_cmd &cmd) {
        if (!bitmap_) {
            ctx.complete(epoc::error_none);
            return;
        }

        std::optional<common::region> reg_clip;
        
        if (cmd.header.op == EWsWinOpUpdateScreenRegion) {
            reg_clip = get_region_from_context(ctx, cmd);
            if (!reg_clip.has_value()) {
                LOG_WARN(SERVICE_WINDOW, "Update screen with region called but failed to retrieve region struct. Nothing is clipped");
            }
        }

        sync_from_bitmap(reg_clip);

        ctx.complete(epoc::error_none);
        canvas_base::try_update(ctx.msg->own_thr);
    }

    bool bitmap_backed_canvas::scroll(eka2l1::rect clip_space, const eka2l1::vec2 offset, eka2l1::rect source_rect) {        
        if (((offset.x == 0) && (offset.y == 0)) || !driver_win_id) {
            return false;
        }

        drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();
        drivers::graphics_command_builder cmd_builder;

        if (!clip_space.empty()) {
            cmd_builder.set_feature(drivers::graphics_feature::clipping, true);
            cmd_builder.clip_bitmap_rect(clip_space);
        }

        if (!ping_pong_driver_win_id) {
            ping_pong_driver_win_id = drivers::create_bitmap(drv, abs_rect.size, 32);
        }

        if (source_rect.empty()) {
            source_rect.top = eka2l1::vec2(0, 0);
            source_rect.size = abs_rect.size;
        }
        
        eka2l1::rect dest_rect(offset, source_rect.size);

        cmd_builder.bind_bitmap(ping_pong_driver_win_id);
        cmd_builder.draw_bitmap(driver_win_id, 0, dest_rect, source_rect);
        cmd_builder.bind_bitmap(driver_win_id);
        cmd_builder.draw_bitmap(ping_pong_driver_win_id, 0, dest_rect, dest_rect);

        drivers::command_list retrieved = cmd_builder.retrieve_command_list();
        drv->submit_command_list(retrieved);

        return true;
    }

    bool bitmap_backed_canvas::draw(drivers::graphics_command_builder &builder) {
        if (!driver_win_id || !can_be_physically_seen()) {
            // No need to redraw this window yet. It doesn't even have any content ready.
            return false;
        }

        // Check if extent is just invalid
        if (size().x == 0 || size().y == 0) {
            // No one can see this. Leave it for now.
            return false;
        }

        if (!content_changed()) {
            if ((scr->flags_ & screen::FLAG_CLIENT_REDRAW_PENDING) && !(scr->flags_ & screen::FLAG_SERVER_REDRAW_PENDING)) {
                return false;
            }
        }

        builder.set_feature(drivers::graphics_feature::blend, false);
        builder.clip_bitmap_region(visible_region, scr->display_scale_factor);

        eka2l1::rect draw_dest_rect = abs_rect;
        scale_rectangle(draw_dest_rect, scr->display_scale_factor);

        if (!scr->scr_config.blt_offscreen && clear_color_enable) {
            auto color_extracted = common::rgba_to_vec(clear_color);

            if (display_mode() <= epoc::display_mode::color16mu) {
                color_extracted.w = 255;
            }

            builder.set_brush_color_detail(color_extracted);
            builder.draw_rectangle(draw_dest_rect);
        }

        builder.set_feature(drivers::graphics_feature::blend, true);

        eka2l1::drivers::filter_option filter = (client->get_ws().get_kernel_system()->get_config()->nearest_neighbor_filtering ?
            eka2l1::drivers::filter_option::nearest : eka2l1::drivers::filter_option::linear);

        builder.set_texture_filter(driver_win_id, false, filter);
        builder.set_texture_filter(driver_win_id, true, filter);
        builder.draw_bitmap(driver_win_id, 0, draw_dest_rect, eka2l1::rect({ 0, 0 }, { 0, 0 }), eka2l1::vec2(0, 0), 0.0f, 0);

        return true;
    }

    bool bitmap_backed_canvas::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {        
        // LOG_TRACE(SERVICE_WINDOW, "Backed up canvas opcode {}", cmd.header.op);

        bool did_it = false;
        const bool should_flush = canvas_base::execute_command_detail(ctx, cmd, did_it);

        if (did_it) {
            return should_flush;
        }

        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);
        switch (op) {
        case EWsWinOpBitmapHandle:
            bitmap_handle(ctx, cmd);
            break;

        case EWsWinOpUpdateScreen:
        case EWsWinOpUpdateScreenRegion:
            update_screen(ctx, cmd);
            break;

        case EWsWinOpMaintainBackup:
            // Ignore, legacy
            ctx.complete(epoc::error_none);
            break;

        default:
            LOG_ERROR(SERVICE_WINDOW, "Unimplemented bitmap backed canavas opcode 0x{:X}!", cmd.header.op);
            break;
        }

        return false;
    }
}
