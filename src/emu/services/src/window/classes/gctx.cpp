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

#include <services/fbs/fbs.h>
#include <services/window/classes/gctx.h>
#include <services/window/classes/scrdvc.h>
#include <services/window/classes/wingroup.h>
#include <services/window/classes/winuser.h>
#include <services/window/op.h>
#include <services/window/opheader.h>
#include <services/window/window.h>

#include <epoc/epoc.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/rgb.h>

#include <utils/err.h>

namespace eka2l1::epoc {
    void graphic_context::active(service::ipc_context &context, ws_cmd cmd) {
        const std::uint32_t window_to_attach_handle = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);
        attached_window = reinterpret_cast<epoc::window_user *>(client->get_object(window_to_attach_handle));

        // Attach context with window
        attached_window->attached_contexts.push(&context_attach_link);

        // Afaik that the pointer to CWsScreenDevice is internal, so not so scared of general users touching
        // this.
        context.complete(attached_window->scr->number);

        drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();

        // Make new command list
        if (!cmd_list) {
            cmd_list = drv->new_command_list();
            cmd_builder = drv->new_command_builder(cmd_list.get());
        }

        // Add first command list, binding our window bitmap
        if (attached_window->driver_win_id == 0) {
            attached_window->driver_win_id = drivers::create_bitmap(drv, attached_window->size);
            attached_window->resize_needed = false;
        }

        recording = true;

        eka2l1::rect viewport;
        viewport.top = { 0, 0 };
        viewport.size = attached_window->size;

        cmd_builder->bind_bitmap(attached_window->driver_win_id);

        if (attached_window->resize_needed) {
            // Try to resize our bitmap. My NVIDIA did forgive me if texture has same spec
            // as before, but not Intel...
            cmd_builder->resize_bitmap(attached_window->driver_win_id, attached_window->size);
        }

        cmd_builder->set_viewport(viewport);
        cmd_builder->clear({ 0, 0, 0, 0 }, drivers::draw_buffer_bit_stencil_buffer);

        do_submit_clipping();
    }

    void graphic_context::do_command_draw_bitmap(service::ipc_context &ctx, drivers::handle h,
        const eka2l1::rect &source_rect, const eka2l1::rect &dest_rect) {
        cmd_builder->draw_bitmap(h, 0, dest_rect, source_rect, 0);
        ctx.complete(epoc::error_none);
    }

    void graphic_context::do_command_draw_text(service::ipc_context &ctx, eka2l1::vec2 top_left,
        eka2l1::vec2 bottom_right, const std::u16string &text, epoc::text_alignment align,
        const int baseline_offset, const int margin) {
        // TODO: Pen outline >_<
        eka2l1::vecx<int, 4> color;
        color = common::rgb_to_vec(pen_color);
        cmd_builder->set_brush_color({ color[1], color[2], color[3] });

        eka2l1::rect area(top_left, bottom_right - top_left);

        // Add the baseline offset. Where text will sit on.
        area.top.y += baseline_offset;

        if (align == epoc::text_alignment::right) {
            area.top.x -= margin;
        } else {
            area.top.x += margin;
        }

        text_font->atlas.draw_text(text, area, align, client->get_ws().get_graphics_driver(),
            cmd_builder.get());

        ctx.complete(epoc::error_none);
    }

    bool graphic_context::do_command_set_brush_color() {
        eka2l1::vecx<int, 4> color = common::rgb_to_vec(brush_color);

        // Don't bother even sending any draw command
        switch (fill_mode) {
        case brush_style::null:
            return false;

        case brush_style::solid:
            if (epoc::is_display_mode_alpha(attached_window->display_mode())) {
                if (color[0] == 0) {
                    // Nothing, dont draw
                    return false;
                }

                cmd_builder->set_brush_color_detail({ color[1], color[2], color[3], color[0] });
            } else {
                cmd_builder->set_brush_color({ color[1], color[2], color[3] });
            }

            break;

        default:
            LOG_WARN("Unhandled brush style {}", static_cast<std::int32_t>(fill_mode));
            return false;
        }

        return true;
    }

    bool graphic_context::do_command_set_pen_color() {
        eka2l1::vecx<int, 4> color = common::rgb_to_vec(pen_color);

        // Don't bother even sending any draw command
        switch (line_mode) {
        case pen_style::null:
            return false;

        case pen_style::solid:
            if (epoc::is_display_mode_alpha(attached_window->display_mode())) {
                if (color[0] == 0) {
                    // Nothing, dont draw
                    return false;
                }

                cmd_builder->set_brush_color_detail({ color[1], color[2], color[3], color[0] });
            } else {
                cmd_builder->set_brush_color({ color[1], color[2], color[3] });
            }

            break;

        default:
            LOG_WARN("Unhandled pen style {}", static_cast<std::int32_t>(fill_mode));
            return false;
        }

        return true;
    }

    void graphic_context::do_submit_clipping() {
        eka2l1::rect the_clip;
        common::region *the_region = nullptr;

        std::unique_ptr<common::region> clip_region_temp = std::make_unique<common::region>();

        bool use_clipping = false;
        bool stencil_one_for_valid = true;

        if (clipping_rect.empty() && clipping_region.empty()) {
            if (attached_window->flags & epoc::window_user::flags_in_redraw) {
                the_clip = attached_window->redraw_rect_curr;
                use_clipping = true;
            } else {
                if (attached_window->redraw_region.empty()) {
                    cmd_builder->set_clipping(false);
                    cmd_builder->set_stencil(false);
                    
                    return;
                }

                use_clipping = false;
                stencil_one_for_valid = false;

                the_region = &attached_window->redraw_region;
            }
        } else {
            if (attached_window->flags & epoc::window_user::flags_in_redraw) {
                clip_region_temp->add_rect(attached_window->redraw_rect_curr);
            } else {
                clip_region_temp->add_rect(attached_window->bounding_rect());
                clip_region_temp->eliminate(attached_window->redraw_region);
            }

            common::region personal_clipping;
            
            if (!clipping_rect.empty()) {
                personal_clipping.add_rect(clipping_rect);
            }

            if (!clipping_region.empty()) {
                personal_clipping = (personal_clipping.empty()) ? clipping_region :
                    personal_clipping.intersect(clipping_region);
            }

            *clip_region_temp = clip_region_temp->intersect(personal_clipping);

            if (clip_region_temp->rects_.size() <= 1) {
                // We can use clipping directly
                use_clipping = true;
                the_clip = clip_region_temp->empty() ? eka2l1::rect({ 0, 0 }, { 0, 0 }) :
                    clip_region_temp->rects_[0];
            } else {
                use_clipping = false;
                stencil_one_for_valid = true;

                the_region = clip_region_temp.get();
            }
        }

        if (use_clipping) {
            cmd_builder->set_clipping(true);

            if (the_clip.valid()) {
                cmd_builder->clip_rect(the_clip);
            }
        } else {
            cmd_builder->set_stencil(true);

            // Try to fill region rects with 1 in stencil buffer.
            // Intentionally let stencil test fail so nothing gets draw. Just need to fill it after all.
            cmd_builder->set_stencil_pass_condition(drivers::stencil_face::back_and_front, drivers::condition_func::never,
                1, 0xFF);
            cmd_builder->set_stencil_action(drivers::stencil_face::back_and_front, drivers::stencil_action::replace,
                drivers::stencil_action::keep, drivers::stencil_action::keep);
            cmd_builder->set_stencil_mask(drivers::stencil_face::back_and_front, 0xFF);

            for (std::size_t i = 0; i < the_region->rects_.size(); i++) {
                if (the_region->rects_[i].valid()) {
                    cmd_builder->draw_rectangle(the_region->rects_[i]);
                }
            }

            // Now set stencil buffer to only pass if value of pixel correspond in stencil buffer is not equal to 1 (invalid region),
            // or equal to 1 (if valid region)
            // Also disable writing to stencil buffer
            cmd_builder->set_stencil_pass_condition(drivers::stencil_face::back_and_front, stencil_one_for_valid ?
                drivers::condition_func::equal : drivers::condition_func::not_equal, 1, 0xFF);
            cmd_builder->set_stencil_action(drivers::stencil_face::back_and_front, drivers::stencil_action::keep,
                drivers::stencil_action::keep, drivers::stencil_action::keep);
        }
    }

    void graphic_context::flush_queue_to_driver() {
        drivers::graphics_driver *driver = client->get_ws().get_graphics_driver();

        // Unbind current bitmap
        cmd_builder->bind_bitmap(0);

        cmd_builder->set_clipping(false);
        cmd_builder->set_stencil(false);

        driver->submit_command_list(*cmd_list);

        // Renew this so that the graphic context can continue
        cmd_list = driver->new_command_list();
        cmd_builder = driver->new_command_builder(cmd_list.get());
    }

    void graphic_context::set_brush_color(service::ipc_context &context, ws_cmd &cmd) {
        brush_color = *reinterpret_cast<const common::rgb *>(cmd.data_ptr);
        context.complete(epoc::error_none);
    }

    void graphic_context::set_pen_color(service::ipc_context &context, ws_cmd &cmd) {
        pen_color = *reinterpret_cast<const common::rgb *>(cmd.data_ptr);
        context.complete(epoc::error_none);
    }

    void graphic_context::deactive(service::ipc_context &context, ws_cmd &cmd) {
        if (attached_window) {
            context_attach_link.deque();

            // Might have to flush sooner, since this window can be used with another
            flush_queue_to_driver();

            // Content of the window changed, so call the handler
            attached_window->take_action_on_change();
        }

        attached_window = nullptr;
        recording = false;

        context.complete(epoc::error_none);
    }

    drivers::handle graphic_context::handle_from_bitwise_bitmap(epoc::bitwise_bitmap *bmp) {
        drivers::graphics_driver *driver = client->get_ws().get_graphics_driver();
        epoc::bitmap_cache *cacher = client->get_ws().get_bitmap_cache();
        return cacher->add_or_get(driver, cmd_builder.get(), bmp);
    }

    void graphic_context::draw_bitmap(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_draw_bitmap *bitmap_cmd = reinterpret_cast<ws_cmd_draw_bitmap *>(cmd.data_ptr);
        epoc::bitwise_bitmap *bw_bmp = client->get_ws().get_bitmap(bitmap_cmd->handle);

        if (!bw_bmp) {
            context.complete(epoc::error_argument);
            return;
        }

        drivers::handle bmp_driver_handle = handle_from_bitwise_bitmap(bw_bmp);
        do_command_draw_bitmap(context, bmp_driver_handle, rect({ 0, 0 }, bw_bmp->header_.size_pixels),
            rect(bitmap_cmd->pos, { 0, 0 }));
    }

    void graphic_context::gdi_blt_masked(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_gdi_blt_masked *blt_cmd = reinterpret_cast<ws_cmd_gdi_blt_masked *>(cmd.data_ptr);
        epoc::bitwise_bitmap *bmp = client->get_ws().get_bitmap(blt_cmd->source_handle);
        epoc::bitwise_bitmap *masked = client->get_ws().get_bitmap(blt_cmd->mask_handle);

        if (!bmp || !masked) {
            context.complete(epoc::error_bad_handle);
            return;
        }

        eka2l1::rect source_rect = blt_cmd->source_rect;
        source_rect.transform_from_symbian_rectangle();

        eka2l1::rect dest_rect;
        dest_rect.size = source_rect.size;
        dest_rect.top = blt_cmd->pos;

        drivers::handle bmp_driver_handle = handle_from_bitwise_bitmap(bmp);
        drivers::handle bmp_mask_driver_handle = handle_from_bitwise_bitmap(masked);

        std::uint32_t flags = 0;
        const bool alpha_blending = (masked->settings_.current_display_mode() == epoc::display_mode::gray256)
            || (epoc::is_display_mode_alpha(masked->settings_.current_display_mode()));

        if (blt_cmd->invert_mask && !alpha_blending) {
            flags |= drivers::bitmap_draw_flag_invert_mask;
        }

        cmd_builder->set_blend_mode(true);
        
        // For non alpha blending we always want to take color buffer's alpha.
        cmd_builder->blend_formula(drivers::blend_equation::add, drivers::blend_equation::add,
            drivers::blend_factor::frag_out_alpha, drivers::blend_factor::one_minus_frag_out_alpha,
            (alpha_blending ? drivers::blend_factor::one : drivers::blend_factor::zero),
            (alpha_blending ? drivers::blend_factor::one_minus_frag_out_alpha : drivers::blend_factor::one));

        bool swizzle_alteration = false;
        if (!alpha_blending && !epoc::is_display_mode_alpha(masked->settings_.current_display_mode())) {
            swizzle_alteration = true;
            cmd_builder->set_swizzle(bmp_mask_driver_handle, drivers::channel_swizzle::red, drivers::channel_swizzle::green,
                drivers::channel_swizzle::blue, drivers::channel_swizzle::red);
        }

        cmd_builder->draw_bitmap(bmp_driver_handle, bmp_mask_driver_handle, dest_rect, source_rect, flags);
        cmd_builder->set_blend_mode(false);

        if (swizzle_alteration) {
            cmd_builder->set_swizzle(bmp_mask_driver_handle, drivers::channel_swizzle::red, drivers::channel_swizzle::green,
                drivers::channel_swizzle::blue, drivers::channel_swizzle::alpha);
        }

        context.complete(epoc::error_none);
    }

    void graphic_context::gdi_blt_impl(service::ipc_context &context, ws_cmd &cmd, const int ver) {
        ws_cmd_gdi_blt3 *blt_cmd = reinterpret_cast<ws_cmd_gdi_blt3 *>(cmd.data_ptr);

        // Try to get the bitmap
        epoc::bitwise_bitmap *bmp = client->get_ws().get_bitmap(blt_cmd->handle);

        if (!bmp) {
            context.complete(epoc::error_bad_handle);
            return;
        }

        eka2l1::rect source_rect;

        if (ver == 2) {
            source_rect.top = { 0, 0 };
            source_rect.size = bmp->header_.size_pixels;
        } else {
            source_rect = blt_cmd->source_rect;
            source_rect.transform_from_symbian_rectangle();
        }

        if (!source_rect.valid()) {
            context.complete(epoc::error_none);
            return;
        }

        eka2l1::rect dest_rect;
        dest_rect.top = blt_cmd->pos;
        dest_rect.size = source_rect.size;

        if ((ver > 2) && (source_rect.size.y > bmp->header_.size_pixels.y)) {
            // The source rect given by the command is too large.
            // By default, the extra space should be filled with white. We will do that by drawing
            // a white rectangle
            cmd_builder->set_brush_color({ 255, 255, 255 });
            cmd_builder->draw_rectangle(dest_rect);

            source_rect.size.y = bmp->header_.size_pixels.y;
            dest_rect.size.y = source_rect.size.y;
        }

        drivers::handle bmp_driver_handle = handle_from_bitwise_bitmap(bmp);
        do_command_draw_bitmap(context, bmp_driver_handle, source_rect, dest_rect);
    }

    void graphic_context::gdi_blt2(service::ipc_context &context, ws_cmd &cmd) {
        gdi_blt_impl(context, cmd, 2);
    }

    void graphic_context::gdi_blt3(service::ipc_context &context, ws_cmd &cmd) {
        gdi_blt_impl(context, cmd, 3);
    }

    void graphic_context::set_brush_style(service::ipc_context &context, ws_cmd &cmd) {
        fill_mode = *reinterpret_cast<brush_style *>(cmd.data_ptr);
        context.complete(epoc::error_none);
    }

    void graphic_context::set_pen_style(service::ipc_context &context, ws_cmd &cmd) {
        line_mode = *reinterpret_cast<pen_style *>(cmd.data_ptr);
        context.complete(epoc::error_none);
    }

    void graphic_context::set_pen_size(service::ipc_context &context, ws_cmd &cmd) {
        pen_size = *reinterpret_cast<eka2l1::vec2 *>(cmd.data_ptr);
        context.complete(epoc::error_none);
    }

    void graphic_context::draw_line(service::ipc_context &context, ws_cmd &cmd) {
        eka2l1::rect area = *reinterpret_cast<eka2l1::rect *>(cmd.data_ptr);

        // Symbian rectangle second vector is the bottom right, not the size
        area.transform_from_symbian_rectangle();

        if (do_command_set_pen_color()) {
            // We want to draw the rectangle that backup the real rectangle, to create borders.
            eka2l1::rect backup_border = area;
            backup_border.top -= pen_size;
            backup_border.size += pen_size;

            cmd_builder->draw_rectangle(backup_border);
        }

        context.complete(epoc::error_none);
    }

    void graphic_context::draw_rect(service::ipc_context &context, ws_cmd &cmd) {
        eka2l1::rect area = *reinterpret_cast<eka2l1::rect *>(cmd.data_ptr);

        // Symbian rectangle second vector is the bottom right, not the size
        area.transform_from_symbian_rectangle();

        if (!area.valid()) {
            context.complete(epoc::error_none);
            return;
        }

        if (do_command_set_pen_color()) {
            // We want to draw the rectangle that backup the real rectangle, to create borders.
            eka2l1::rect backup_border = area;
            backup_border.top -= pen_size;
            backup_border.size += pen_size * 2;

            cmd_builder->draw_rectangle(backup_border);
        }

        // Draw the real rectangle! Hurray!
        if (do_command_set_brush_color()) {
            cmd_builder->draw_rectangle(area);
        }

        context.complete(epoc::error_none);
    }

    void graphic_context::clear(service::ipc_context &context, ws_cmd &cmd) {
        eka2l1::rect area(attached_window->pos, attached_window->size);

        if (!area.valid()) {
            context.complete(epoc::error_none);
            return;
        }

        const auto previous_brush_type = fill_mode;
        fill_mode = brush_style::solid;

        if (do_command_set_brush_color()) {
            cmd_builder->draw_rectangle(area);
        }

        fill_mode = previous_brush_type;

        // Draw rectangle
        context.complete(epoc::error_none);
    }

    void graphic_context::clear_rect(service::ipc_context &context, ws_cmd &cmd) {
        eka2l1::rect area = *reinterpret_cast<eka2l1::rect *>(cmd.data_ptr);

        // Symbian rectangle second vector is the bottom right, not the size
        area.transform_from_symbian_rectangle();

        if (!area.valid()) {
            context.complete(epoc::error_none);
            return;
        }

        const auto previous_brush_type = fill_mode;
        fill_mode = brush_style::solid;

        if (do_command_set_brush_color()) {
            cmd_builder->draw_rectangle(area);
        }

        fill_mode = previous_brush_type;

        // Draw rectangle
        context.complete(epoc::error_none);
    }

    void graphic_context::reset_context() {
        if (text_font) {
            text_font->deref();
            text_font = nullptr;
        }

        fill_mode = brush_style::null;
        line_mode = pen_style::null;

        pen_size = { 1, 1 };
        brush_color = 0;

        clipping_rect.make_empty();
        clipping_region.make_empty();

        do_submit_clipping();
    }

    void graphic_context::reset(service::ipc_context &context, ws_cmd &cmd) {
        reset_context();
        context.complete(epoc::error_none);
    }

    void graphic_context::use_font(service::ipc_context &context, ws_cmd &cmd) {
        service::uid font_handle = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);
        fbs_server *fbs = client->get_ws().get_fbs_server();

        fbsfont *font_object = fbs->get_font(font_handle);

        if (!font_object) {
            context.complete(epoc::error_argument);
            return;
        }

        text_font = font_object;
        text_font->ref();

        if (text_font->atlas.atlas_handle_ == 0) {
            // Initialize the atlas
            text_font->atlas.init(font_object->of_info.adapter, text_font->of_info.idx, 0x20, 0xFF - 0x20, font_object->of_info.metrics.max_height);
        }

        context.complete(epoc::error_none);
    }

    void graphic_context::discard_font(service::ipc_context &context, ws_cmd &cmd) {
        if (text_font) {
            text_font->deref();
            text_font = nullptr;
        }
    }

    void graphic_context::draw_text(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_draw_text *info = reinterpret_cast<decltype(info)>(cmd.data_ptr);
        std::u16string text(reinterpret_cast<char16_t *>(reinterpret_cast<std::uint8_t *>(cmd.data_ptr) + sizeof(ws_cmd_draw_text)), info->length);

        do_command_draw_text(context, info->pos, info->pos, text,
            epoc::text_alignment::left, 0, 0);
    }

    void graphic_context::draw_box_text_optimised1(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_draw_box_text_optimised1 *info = reinterpret_cast<decltype(info)>(cmd.data_ptr);
        std::u16string text(reinterpret_cast<char16_t *>(reinterpret_cast<std::uint8_t *>(cmd.data_ptr) + sizeof(ws_cmd_draw_box_text_optimised1)), info->length);

        do_command_draw_text(context, info->left_top_pos, info->right_bottom_pos, text,
            epoc::text_alignment::left, info->baseline_offset, 0);
    }

    void graphic_context::draw_box_text_optimised2(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_draw_box_text_optimised2 *info = reinterpret_cast<decltype(info)>(cmd.data_ptr);
        std::u16string text(reinterpret_cast<char16_t *>(reinterpret_cast<std::uint8_t *>(cmd.data_ptr) + sizeof(ws_cmd_draw_box_text_optimised2)), info->length);

        do_command_draw_text(context, info->left_top_pos, info->right_bottom_pos, text,
            info->horiz, info->baseline_offset, info->left_mgr);
    }
    
    void graphic_context::set_clipping_rect(service::ipc_context &context, ws_cmd &cmd) {
        eka2l1::rect the_clip = *reinterpret_cast<eka2l1::rect*>(cmd.data_ptr);
        the_clip.transform_from_symbian_rectangle();

        clipping_rect = the_clip;
        do_submit_clipping();

        context.complete(epoc::error_none);
    }

    void graphic_context::free(service::ipc_context &context, ws_cmd &cmd) {
        context.complete(epoc::error_none);
        client->delete_object(cmd.obj_handle);
    }

    void graphic_context::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        //LOG_TRACE("Graphics context opcode {}", cmd.header.op);
        ws_graphics_context_opcode op = static_cast<decltype(op)>(cmd.header.op);

        // General rules: Stub to err_none = nullptr, implement = function pointer
        //                Do nothing = add nothing
        using ws_graphics_context_op_handler = std::function<void(graphic_context *,
            service::ipc_context & ctx, ws_cmd & cmd)>;

        using ws_graphics_context_table_op = std::map<ws_graphics_context_opcode, ws_graphics_context_op_handler>;

        static const ws_graphics_context_table_op v171u_opcode_handlers = {
            { ws_gc_u171_active, &graphic_context::active },
            { ws_gc_u171_set_clipping_rect, &graphic_context::set_clipping_rect },
            { ws_gc_u171_set_brush_color, &graphic_context::set_brush_color },
            { ws_gc_u171_set_brush_style, &graphic_context::set_brush_style },
            { ws_gc_u171_set_pen_color, &graphic_context::set_pen_color },
            { ws_gc_u171_set_pen_style, &graphic_context::set_pen_style },
            { ws_gc_u171_set_pen_size, &graphic_context::set_pen_size },
            { ws_gc_u171_deactive, &graphic_context::deactive },
            { ws_gc_u171_reset, &graphic_context::reset },
            { ws_gc_u171_use_font, &graphic_context::use_font },
            { ws_gc_u171_discard_font, &graphic_context::discard_font },
            { ws_gc_u171_draw_line, &graphic_context::draw_line },
            { ws_gc_u171_draw_rect, &graphic_context::draw_rect },
            { ws_gc_u171_clear, &graphic_context::clear },
            { ws_gc_u171_clear_rect, &graphic_context::clear_rect },
            { ws_gc_u171_draw_bitmap, &graphic_context::draw_bitmap },
            { ws_gc_u171_draw_text, &graphic_context::draw_text },
            { ws_gc_u171_draw_box_text_optimised1, &graphic_context::draw_box_text_optimised1 },
            { ws_gc_u171_draw_box_text_optimised2, &graphic_context::draw_box_text_optimised2 },
            { ws_gc_u171_gdi_blt2, &graphic_context::gdi_blt2 },
            { ws_gc_u171_gdi_blt3, &graphic_context::gdi_blt3 },
            { ws_gc_u171_gdi_blt_masked, &graphic_context::gdi_blt_masked },
            { ws_gc_u171_free, &graphic_context::free }
        };

        static const ws_graphics_context_table_op curr_opcode_handlers = {
            { ws_gc_curr_active, &graphic_context::active },
            { ws_gc_curr_set_clipping_rect, &graphic_context::set_clipping_rect },
            { ws_gc_curr_set_brush_color, &graphic_context::set_brush_color },
            { ws_gc_curr_set_brush_style, &graphic_context::set_brush_style },
            { ws_gc_curr_set_pen_color, &graphic_context::set_pen_color },
            { ws_gc_curr_set_pen_style, &graphic_context::set_pen_style },
            { ws_gc_curr_set_pen_size, &graphic_context::set_pen_size },
            { ws_gc_curr_deactive, &graphic_context::deactive },
            { ws_gc_curr_reset, &graphic_context::reset },
            { ws_gc_curr_use_font, &graphic_context::use_font },
            { ws_gc_curr_discard_font, &graphic_context::discard_font },
            { ws_gc_curr_draw_line, &graphic_context::draw_line },
            { ws_gc_curr_draw_rect, &graphic_context::draw_rect },
            { ws_gc_curr_clear, &graphic_context::clear },
            { ws_gc_curr_clear_rect, &graphic_context::clear_rect },
            { ws_gc_curr_draw_bitmap, &graphic_context::draw_bitmap },
            { ws_gc_curr_draw_text, &graphic_context::draw_text },
            { ws_gc_curr_draw_box_text_optimised1, &graphic_context::draw_box_text_optimised1 },
            { ws_gc_curr_draw_box_text_optimised2, &graphic_context::draw_box_text_optimised2 },
            { ws_gc_curr_gdi_blt2, &graphic_context::gdi_blt2 },
            { ws_gc_curr_gdi_blt3, &graphic_context::gdi_blt3 },
            { ws_gc_curr_gdi_blt_masked, &graphic_context::gdi_blt_masked },
            { ws_gc_curr_free, &graphic_context::free }
        };

        epoc::version cli_ver = client->client_version();
        ws_graphics_context_op_handler handler = nullptr;

#define FIND_OPCODE(op, table)                                               \
    auto result = table.find(op);                                            \
    if (result == table.end() || !result->second) {                          \
        LOG_WARN("Unimplemented graphics context opcode {}", cmd.header.op); \
        return;                                                              \
    }                                                                        \
    handler = result->second;

        if (cli_ver.major == 1 && cli_ver.minor == 0) {
            if (cli_ver.build <= 171) {
                // Execute table 1
                FIND_OPCODE(op, v171u_opcode_handlers)
            } else {
                // Execute table 2
                FIND_OPCODE(op, curr_opcode_handlers)
            }
        }

        handler(this, ctx, cmd);
    }

    graphic_context::graphic_context(window_server_client_ptr client, epoc::window *attach_win)
        : window_client_obj(client, nullptr)
        , attached_window(reinterpret_cast<epoc::window_user *>(attach_win))
        , text_font(nullptr)
        , fill_mode(brush_style::null)
        , line_mode(pen_style::null)
        , brush_color(0)
        , pen_color(0)
        , pen_size(1, 1)
        , cmd_list(nullptr)
        , cmd_builder(nullptr) {
    }
}
