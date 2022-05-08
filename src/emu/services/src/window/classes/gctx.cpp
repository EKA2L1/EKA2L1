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
#include <services/window/classes/bitmap.h>
#include <services/window/classes/gctx.h>
#include <services/window/classes/scrdvc.h>
#include <services/window/classes/wingroup.h>
#include <services/window/classes/winuser.h>
#include <services/window/op.h>
#include <services/window/opheader.h>
#include <services/window/util.h>
#include <services/window/window.h>

#include <system/epoc.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/rgb.h>

#include <utils/err.h>

namespace eka2l1::epoc {
    static void *decide_bitmap_pointer_to_pass(wsbitmap *server_bmp, std::uint8_t &affected_flags, const bool is_mask) {
        if (server_bmp->parent_) {
            return server_bmp->parent_;
        }

        affected_flags |= (is_mask ? GDI_STORE_COMMAND_MASK_RAW : GDI_STORE_COMMAND_MAIN_RAW);
        return server_bmp->bitmap_;
    }

    bool graphic_context::no_building() const {
        return !attached_window || (attached_window->abs_rect.size == eka2l1::vec2(0, 0));
    }

    void graphic_context::active(service::ipc_context &context, ws_cmd cmd) {
        const std::uint32_t window_to_attach_handle = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);
        attached_window = reinterpret_cast<epoc::canvas_base *>(client->get_object(window_to_attach_handle));

        // Attach context with window
        attached_window->attached_contexts.push(&context_attach_link);

        // Afaik that the pointer to CWsScreenDevice is internal, so not so scared of general users touching
        // this.
        context.complete(attached_window->scr->number);

        if (no_building()) {
            return;
        }

        drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();

        // Add first command list, binding our window bitmap
        // We already completed our request, so it's no fear to unlock the kernel now!
        kernel_system *kern = context.sys->get_kernel_system();
        kern->unlock();

        attached_window->prepare_for_draw();

        kern->lock();
        recording = true;

        if (attached_window->win_type == epoc::window_type::backed_up) {
            epoc::bitmap_backed_canvas *cv = reinterpret_cast<epoc::bitmap_backed_canvas*>(attached_window);
            cv->sync_from_bitmap();
        }

        // Reset clipping. This is not mentioned in doc but is in official source code.
        // See gc.cpp file. Opcode EWsGcOpActivate
        clipping_rect.make_empty();
        clipping_region.make_empty();

        // In source code brush is also resetted
        brush_color = attached_window->clear_color;

        do_submit_clipping();
    }

    void graphic_context::do_command_draw_bitmap(service::ipc_context &ctx, void *bitmap, eka2l1::rect source_rect, eka2l1::rect dest_rect, const std::uint8_t flags) {
        epoc::gdi_store_command draw_cmd;
        epoc::gdi_store_command_draw_bitmap_data &draw_data = draw_cmd.get_data_struct<epoc::gdi_store_command_draw_bitmap_data>();

        draw_cmd.opcode_ = epoc::gdi_store_command_draw_bitmap;
        draw_data.dest_rect_ = dest_rect;
        draw_data.source_rect_ = source_rect;
        draw_data.gdi_flags_ = flags;
        draw_data.main_fbs_bitmap_ = bitmap;
        draw_data.mask_fbs_bitmap_ = nullptr;
        draw_data.main_drv_ = 0;
        draw_data.mask_drv_ = 0;

        attached_window->add_draw_command(draw_cmd);
        ctx.complete(epoc::error_none);
    }

    void graphic_context::do_command_draw_text(service::ipc_context &ctx, eka2l1::vec2 top_left,
        eka2l1::vec2 bottom_right, const std::u16string &text, epoc::text_alignment align,
        const int baseline_offset, const int margin, const bool fill_surrounding) {
        // Complete it first, cause we gonna open up the kernel
        ctx.complete(epoc::error_none);

        eka2l1::rect area(top_left, bottom_right - top_left);
        eka2l1::vec4 color_brush;

        epoc::gdi_store_command submit_cmd;
        
        if (fill_surrounding) {
            // The effective box colour depends on the drawing mode. As the document says
            if (get_brush_color(color_brush)) {
                epoc::gdi_store_command_draw_rect_data &text_box_clear_data = submit_cmd.get_data_struct<epoc::gdi_store_command_draw_rect_data>();

                text_box_clear_data.rect_ = area;
                text_box_clear_data.color_ = color_brush;
                submit_cmd.opcode_ = epoc::gdi_store_command_draw_rect;

                attached_window->add_draw_command(submit_cmd);
            }
        }

        // Add the baseline offset. Where text will sit on.
        area.top.y += baseline_offset;

        if (align == epoc::text_alignment::right) {
            area.top.x -= margin;
        } else {
            area.top.x += margin;
        }

        kernel_system *kern = client->get_ws().get_kernel_system();

        epoc::gdi_store_command draw_text_cmd;
        epoc::gdi_store_command_draw_text_data &draw_text_data = draw_text_cmd.get_data_struct<epoc::gdi_store_command_draw_text_data>();

        draw_text_cmd.opcode_ = epoc::gdi_store_command_draw_text;
        draw_text_data.string_ = reinterpret_cast<char16_t*>(draw_text_cmd.allocate_dynamic_data((text.length() + 1) * sizeof(char16_t)));

        std::memcpy(draw_text_data.string_, text.data(), (text.length() + 1) * sizeof(char16_t));

        draw_text_data.alignment_ = static_cast<std::uint32_t>(align);
        draw_text_data.text_box_ = area;
        draw_text_data.fbs_font_ptr_ = text_font;
        draw_text_data.color_ = common::rgba_to_vec(pen_color);
        draw_text_data.color_.w = 255;

        attached_window->add_draw_command(draw_text_cmd);
    }

    bool graphic_context::get_brush_color(eka2l1::vec4 &color_result) {
        color_result = common::rgba_to_vec(brush_color);

        // Don't bother even sending any draw command
        switch (fill_mode) {
        case brush_style::null:
            return false;

        case brush_style::solid:
            if (epoc::is_display_mode_alpha(attached_window->display_mode())) {
                if (color_result.w == 0) {
                    // Nothing, dont draw
                    return false;
                }
            } else {
                color_result.w = 255;
            }

            break;

        default:
            LOG_WARN(SERVICE_WINDOW, "Unhandled brush style {}", static_cast<std::int32_t>(fill_mode));
            return false;
        }

        return true;
    }

    bool graphic_context::get_pen_color_and_style(eka2l1::vec4 &pen_color_result, drivers::pen_style &style) {
        pen_color_result = common::rgba_to_vec(pen_color);

        // Don't bother even sending any draw command
        switch (line_mode) {
        case pen_style::null:
            return false;

        case pen_style::solid:
        case pen_style::dotted:
        case pen_style::dashed:
        case pen_style::dot_dot_dash:
        case pen_style::dot_dash:
            if (epoc::is_display_mode_alpha(attached_window->display_mode())) {
                if (pen_color_result.w == 0) {
                    // Nothing, dont draw
                    return false;
                }
            } else {
                pen_color_result.w = 255;
            }

            // NOTE: Literally same translation for now.
            style = static_cast<drivers::pen_style>(line_mode);

            break;

        default:
            LOG_WARN(SERVICE_WINDOW, "Unhandled pen style {}", static_cast<std::int32_t>(line_mode));
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

        epoc::gdi_store_command cmd;

        if (clipping_rect.empty() && clipping_region.empty()) {
            if (attached_window->flags & epoc::canvas_base::flags_in_redraw) {
                epoc::redraw_msg_canvas *attached_fm_window = reinterpret_cast<epoc::redraw_msg_canvas*>(attached_window);
                the_clip = attached_fm_window->redraw_rect_curr;
                use_clipping = true;
            } else {
                // Developement document says that when not being redrawn, drawing is clipped to non-invalid part.
                // But so far, I have not been able to see any open source code points to that being true. Even simple test can prove that's false.
                // So for now, we let drawing happens on the window with no restrictions. Invalid region will still be invalidated.
                cmd.opcode_ = gdi_store_command_disable_clip;
                attached_window->add_draw_command(cmd);

                return;
            }
        } else {
            if (attached_window->flags & epoc::canvas_base::flags_in_redraw) {
                epoc::redraw_msg_canvas *attached_fm_window = reinterpret_cast<epoc::redraw_msg_canvas*>(attached_window);
                clip_region_temp->add_rect(attached_fm_window->redraw_rect_curr);
            } else {
                clip_region_temp->add_rect(attached_window->bounding_rect());

                // Following the upper comment, assume there's no invalid region.
                // clip_region_temp->eliminate(attached_window->redraw_region);
            }

            common::region personal_clipping;

            if (!clipping_rect.empty()) {
                personal_clipping.add_rect(clipping_rect);
            }

            if (!clipping_region.empty()) {
                personal_clipping = (personal_clipping.empty()) ? clipping_region : personal_clipping.intersect(clipping_region);
            }

            *clip_region_temp = clip_region_temp->intersect(personal_clipping);

            if (clip_region_temp->rects_.size() <= 1) {
                // We can use clipping directly
                use_clipping = true;
                the_clip = clip_region_temp->empty() ? eka2l1::rect({ 0, 0 }, { 0, 0 }) : clip_region_temp->rects_[0];
            } else {
                use_clipping = false;
                stencil_one_for_valid = true;

                the_region = clip_region_temp.get();
            }
        }

        if (use_clipping) {
            cmd.opcode_ = epoc::gdi_store_command_set_clip_rect_single;

            epoc::gdi_store_command_set_clip_rect_single_data &data = cmd.get_data_struct<epoc::gdi_store_command_set_clip_rect_single_data>();
            data.clipping_rect_ = the_clip;
        } else {
            cmd.opcode_ = epoc::gdi_store_command_set_clip_rect_multiple;
            epoc::gdi_store_command_set_clip_rect_multiple_data &data = cmd.get_data_struct<epoc::gdi_store_command_set_clip_rect_multiple_data>();

            data.rect_count_ = static_cast<std::uint32_t>(the_region->rects_.size());
            data.rects_ = reinterpret_cast<eka2l1::rect*>(cmd.allocate_dynamic_data(data.rect_count_ * sizeof(eka2l1::rect)));

            std::memcpy(data.rects_, the_region->rects_.data(), data.rect_count_ * sizeof(eka2l1::rect));
        }

        attached_window->add_draw_command(cmd);
    }

    void graphic_context::set_brush_color(service::ipc_context &context, ws_cmd &cmd) {
        kernel_system *kern = context.sys->get_kernel_system();
        brush_color = *reinterpret_cast<const common::rgba *>(cmd.data_ptr);

        if (!kern->is_eka1()) {
            // From EKA2, color that passed through the server is 0xaarrggbb. R and B channels are swapped
            // The call that makes the color is TRgb::Internal()
            brush_color = (brush_color & 0xFF00FF00) | ((brush_color & 0xFF) << 16) | ((brush_color & 0xFF0000) >> 16);
        }

        context.complete(epoc::error_none);
    }

    void graphic_context::set_pen_color(service::ipc_context &context, ws_cmd &cmd) {
        kernel_system *kern = context.sys->get_kernel_system();
        pen_color = *reinterpret_cast<const common::rgba *>(cmd.data_ptr);

        if (!kern->is_eka1()) {
            // From EKA2, color that passed through the server is 0xaarrggbb. R and B channels are swapped
            // The call that makes the color is TRgb::Internal()
            pen_color = (pen_color & 0xFF00FF00) | ((pen_color & 0xFF) << 16) | ((pen_color & 0xFF0000) >> 16);
        }

        context.complete(epoc::error_none);
    }

    void graphic_context::on_command_batch_done(service::ipc_context &ctx) {
        if (attached_window) {
            submit_queue_commands(ctx.msg->own_thr);
        }
    }

    void graphic_context::submit_queue_commands(kernel::thread *rq) {
        if (!flushed) {
            // Content of the window changed, so call the handler
            attached_window->try_update(rq);
            flushed = true;
        }
    }

    void graphic_context::deactive(service::ipc_context &context, ws_cmd &cmd) {
        if (attached_window) {
            context_attach_link.deque();

            // Might have to flush sooner, since this window can be used with another
            submit_queue_commands(context.msg->own_thr);
        }

        attached_window = nullptr;
        recording = false;

        context.complete(epoc::error_none);
    }

    void graphic_context::draw_bitmap(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_draw_bitmap *bitmap_cmd = reinterpret_cast<ws_cmd_draw_bitmap *>(cmd.data_ptr);
        fbsbitmap *bw_bmp = client->get_ws().get_raw_fbsbitmap(bitmap_cmd->handle);

        if (!bw_bmp) {
            context.complete(epoc::error_argument);
            return;
        }

        do_command_draw_bitmap(context, bw_bmp, rect({ 0, 0 }, { 0, 0 }), rect(bitmap_cmd->pos, { 0, 0 }), 0);
    }

    void graphic_context::draw_bitmap_2(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_draw_bitmap2 *bitmap_cmd = reinterpret_cast<ws_cmd_draw_bitmap2 *>(cmd.data_ptr);
        fbsbitmap *bw_bmp = client->get_ws().get_raw_fbsbitmap(bitmap_cmd->handle);

        if (!bw_bmp) {
            context.complete(epoc::error_argument);
            return;
        }

        eka2l1::rect dest_rect = bitmap_cmd->dest_rect;
        dest_rect.transform_from_symbian_rectangle();

        if (dest_rect.empty()) {
            context.complete(epoc::error_none);
            return;
        }

        do_command_draw_bitmap(context, bw_bmp, rect({ 0, 0 }, { 0, 0 }), dest_rect, 0);
    }

    void graphic_context::draw_bitmap_3(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_draw_bitmap3 *bitmap_cmd = reinterpret_cast<ws_cmd_draw_bitmap3 *>(cmd.data_ptr);
        fbsbitmap *bw_bmp = client->get_ws().get_raw_fbsbitmap(bitmap_cmd->handle);

        if (!bw_bmp) {
            context.complete(epoc::error_argument);
            return;
        }

        eka2l1::rect source_rect = bitmap_cmd->source_rect;
        eka2l1::rect dest_rect = bitmap_cmd->dest_rect;

        source_rect.transform_from_symbian_rectangle();
        dest_rect.transform_from_symbian_rectangle();

        if (source_rect.empty() || dest_rect.empty()) {
            context.complete(epoc::error_none);
            return;
        }

        do_command_draw_bitmap(context, bw_bmp, source_rect, dest_rect, 0);
    }

    void graphic_context::draw_mask_impl(void *source_bitmap, void *mask_bitmap, eka2l1::rect dest_rect, eka2l1::rect source_rect, const std::uint8_t extra_flags) {
        epoc::gdi_store_command draw_bmp_cmd;
        epoc::gdi_store_command_draw_bitmap_data &draw_data = draw_bmp_cmd.get_data_struct<gdi_store_command_draw_bitmap_data>();

        draw_bmp_cmd.opcode_ = epoc::gdi_store_command_draw_bitmap;

        draw_data.dest_rect_ = dest_rect;
        draw_data.source_rect_ = source_rect;
        draw_data.main_fbs_bitmap_ = source_bitmap;
        draw_data.mask_fbs_bitmap_ = mask_bitmap;
        draw_data.main_drv_ = 0;
        draw_data.mask_drv_ = 0;
        draw_data.gdi_flags_ = extra_flags;

        attached_window->add_draw_command(draw_bmp_cmd);
    }

    void graphic_context::ws_draw_bitmap_masked(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_draw_ws_bitmap_masked *blt_cmd = reinterpret_cast<ws_cmd_draw_ws_bitmap_masked *>(cmd.data_ptr);

        epoc::wsbitmap *myside_source_bmp = reinterpret_cast<epoc::wsbitmap *>(client->get_object(blt_cmd->source_handle));
        epoc::wsbitmap *myside_mask_bmp = reinterpret_cast<epoc::wsbitmap *>(client->get_object(blt_cmd->mask_handle));

        if (!myside_source_bmp || !myside_mask_bmp) {
            context.complete(epoc::error_bad_handle);
            return;
        }

        eka2l1::rect source_rect = blt_cmd->source_rect;
        eka2l1::rect dest_rect = blt_cmd->dest_rect;

        source_rect.transform_from_symbian_rectangle();
        dest_rect.transform_from_symbian_rectangle();

        std::uint8_t flags = 0;

        void *source_bmp_to_pass = decide_bitmap_pointer_to_pass(myside_source_bmp, flags, false);
        void *mask_bmp_to_pass = decide_bitmap_pointer_to_pass(myside_mask_bmp, flags, true);

        if (static_cast<bool>(blt_cmd->invert_mask)) {
            flags |= GDI_STORE_COMMAND_INVERT_MASK;
        }

        draw_mask_impl(source_bmp_to_pass, mask_bmp_to_pass, dest_rect, source_rect, flags);
        context.complete(epoc::error_none);
    }

    void graphic_context::gdi_blt_masked(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_gdi_blt_masked *blt_cmd = reinterpret_cast<ws_cmd_gdi_blt_masked *>(cmd.data_ptr);
        fbsbitmap *bmp = client->get_ws().get_raw_fbsbitmap(blt_cmd->source_handle);
        fbsbitmap *masked = client->get_ws().get_raw_fbsbitmap(blt_cmd->mask_handle);

        if (!bmp || !masked) {
            context.complete(epoc::error_bad_handle);
            return;
        }

        eka2l1::rect source_rect = blt_cmd->source_rect;
        source_rect.transform_from_symbian_rectangle();

        eka2l1::rect dest_rect;
        dest_rect.size = eka2l1::vec2(0, 0);
        dest_rect.top = blt_cmd->pos;

        std::uint8_t flags = GDI_STORE_COMMAND_BLIT;
        if (blt_cmd->invert_mask)
            flags |= 1;

        draw_mask_impl(bmp, masked, dest_rect, source_rect, flags);
        context.complete(epoc::error_none);
    }

    void graphic_context::gdi_ws_blt_masked(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_gdi_blt_masked *blt_cmd = reinterpret_cast<ws_cmd_gdi_blt_masked *>(cmd.data_ptr);

        epoc::wsbitmap *myside_source_bmp = reinterpret_cast<epoc::wsbitmap *>(client->get_object(blt_cmd->source_handle));
        epoc::wsbitmap *myside_mask_bmp = reinterpret_cast<epoc::wsbitmap *>(client->get_object(blt_cmd->mask_handle));

        if (!myside_source_bmp || !myside_mask_bmp) {
            context.complete(epoc::error_bad_handle);
            return;
        }

        eka2l1::rect source_rect = blt_cmd->source_rect;
        source_rect.transform_from_symbian_rectangle();

        eka2l1::rect dest_rect;
        dest_rect.size = eka2l1::vec2(0, 0);
        dest_rect.top = blt_cmd->pos;

        std::uint8_t flags = GDI_STORE_COMMAND_BLIT;
        if (static_cast<bool>(blt_cmd->invert_mask)) {
            flags |= GDI_STORE_COMMAND_INVERT_MASK;
        }

        void *source_bmp_to_pass = decide_bitmap_pointer_to_pass(myside_source_bmp, flags, false);
        void *mask_bmp_to_pass = decide_bitmap_pointer_to_pass(myside_mask_bmp, flags, true);

        draw_mask_impl(source_bmp_to_pass, mask_bmp_to_pass, dest_rect, source_rect, flags);
        context.complete(epoc::error_none);
    }

    void graphic_context::gdi_blt_impl(service::ipc_context &context, ws_cmd &cmd, const int ver, const bool ws) {
        ws_cmd_gdi_blt3 *blt_cmd = reinterpret_cast<ws_cmd_gdi_blt3 *>(cmd.data_ptr);

        eka2l1::rect source_rect;

        if (ver == 2) {
            source_rect.top = { 0, 0 };
            source_rect.size = eka2l1::object_size(0, 0);
        } else {
            source_rect = blt_cmd->source_rect;
            source_rect.transform_from_symbian_rectangle();

            if (source_rect.empty()) {
                context.complete(epoc::error_none);
                return;
            }
        }

        // Try to get the bitmap
        void *bmp = nullptr;
        std::uint8_t flags = GDI_STORE_COMMAND_BLIT;

        if (ws) {
            epoc::wsbitmap *myside_bmp = reinterpret_cast<epoc::wsbitmap *>(client->get_object(blt_cmd->handle));

            if (!myside_bmp) {
                context.complete(epoc::error_bad_handle);
                return;
            }

            bmp = decide_bitmap_pointer_to_pass(myside_bmp, flags, false);
        } else {
            bmp = client->get_ws().get_raw_fbsbitmap(blt_cmd->handle);
        }

        if (!bmp) {
            context.complete(epoc::error_bad_handle);
            return;
        }

        eka2l1::rect dest_rect;
        dest_rect.top = blt_cmd->pos;
        dest_rect.size = eka2l1::vec2(0, 0);

        do_command_draw_bitmap(context, bmp, source_rect, dest_rect, flags);
    }

    void graphic_context::gdi_blt2(service::ipc_context &context, ws_cmd &cmd) {
        gdi_blt_impl(context, cmd, 2, false);
    }

    void graphic_context::gdi_blt3(service::ipc_context &context, ws_cmd &cmd) {
        gdi_blt_impl(context, cmd, 3, false);
    }

    void graphic_context::gdi_ws_blt2(service::ipc_context &context, ws_cmd &cmd) {
        gdi_blt_impl(context, cmd, 2, true);
    }

    void graphic_context::gdi_ws_blt3(service::ipc_context &context, ws_cmd &cmd) {
        gdi_blt_impl(context, cmd, 3, true);
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
        drivers::pen_style pen_style;
        eka2l1::vec4 pen_color;

        if (get_pen_color_and_style(pen_color, pen_style)) {
            epoc::gdi_store_command cmd;
            epoc::gdi_store_command_draw_line_data &draw_line_data = cmd.get_data_struct<epoc::gdi_store_command_draw_line_data>();

            cmd.opcode_ = epoc::gdi_store_command_draw_line;
            draw_line_data.color_ = pen_color;
            draw_line_data.style_ = pen_style;

            // It's actually two points
            draw_line_data.start_ = area.top;
            draw_line_data.end_ = area.size;
            draw_line_data.pen_size_ = pen_size;

            attached_window->add_draw_command(cmd);
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

        drivers::pen_style pen_style;
        eka2l1::vec4 pen_color;
        epoc::gdi_store_command gdi_cmd;

        if (get_pen_color_and_style(pen_color, pen_style)) {
            eka2l1::vec2 point_list[5] =  {
                area.top,
                area.top + eka2l1::vec2(area.size.x, 0),
                area.top + area.size,
                area.top + eka2l1::vec2(0, area.size.y),
                area.top
            };

            epoc::gdi_store_command_draw_polygon_data &cmd_data = gdi_cmd.get_data_struct<epoc::gdi_store_command_draw_polygon_data>();

            gdi_cmd.opcode_ = epoc::gdi_store_command_draw_polygon;
            cmd_data.point_count_ = 5;
            cmd_data.color_ = pen_color;
            cmd_data.style_ = pen_style;
            cmd_data.points_ = reinterpret_cast<eka2l1::point*>(gdi_cmd.allocate_dynamic_data(5 * sizeof(eka2l1::point)));

            std::memcpy(cmd_data.points_, point_list, 5 * sizeof(eka2l1::point));

            attached_window->add_draw_command(gdi_cmd);
        }

        // Draw the real rectangle! Hurray!
        if (get_brush_color(pen_color)) {
            epoc::gdi_store_command_draw_rect_data &rect_draw_data = gdi_cmd.get_data_struct<epoc::gdi_store_command_draw_rect_data>();

            rect_draw_data.rect_ = area;
            rect_draw_data.color_ = pen_color;
            gdi_cmd.opcode_ = epoc::gdi_store_command_draw_rect;

            attached_window->add_draw_command(gdi_cmd);
        }

        context.complete(epoc::error_none);
    }

    void graphic_context::clear(service::ipc_context &context, ws_cmd &cmd) {
        eka2l1::rect area(attached_window->pos, attached_window->size());

        if (!area.valid()) {
            context.complete(epoc::error_none);
            return;
        }

        epoc::gdi_store_command gdi_cmd;
        epoc::gdi_store_command_draw_rect_data &rect_draw_data = gdi_cmd.get_data_struct<epoc::gdi_store_command_draw_rect_data>();

        rect_draw_data.rect_ = area;
        rect_draw_data.color_ = common::rgba_to_vec(brush_color);
        gdi_cmd.opcode_ = epoc::gdi_store_command_draw_rect;

        if (!epoc::is_display_mode_alpha(attached_window->display_mode())) {
            rect_draw_data.color_.w = 255;
        }

        attached_window->add_draw_command(gdi_cmd);

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

        epoc::gdi_store_command gdi_cmd;
        epoc::gdi_store_command_draw_rect_data &rect_draw_data = gdi_cmd.get_data_struct<epoc::gdi_store_command_draw_rect_data>();

        rect_draw_data.rect_ = area;
        rect_draw_data.color_ = common::rgba_to_vec(brush_color);
        gdi_cmd.opcode_ = epoc::gdi_store_command_draw_rect;

        if (!epoc::is_display_mode_alpha(attached_window->display_mode())) {
            rect_draw_data.color_.w = 255;
        }
        
        attached_window->add_draw_command(gdi_cmd);

        // Draw rectangle
        context.complete(epoc::error_none);
    }

    void graphic_context::plot(service::ipc_context &context, ws_cmd &cmd) {
        eka2l1::point pos = *reinterpret_cast<eka2l1::point*>(cmd.data_ptr);
        
        // For now emulate it with drawing a 1x1 rectangle
        epoc::gdi_store_command gdi_cmd;
        epoc::gdi_store_command_draw_rect_data &rect_draw_data = gdi_cmd.get_data_struct<epoc::gdi_store_command_draw_rect_data>();

        rect_draw_data.rect_ = eka2l1::rect(pos, eka2l1::vec2(1, 1));
        rect_draw_data.color_ = common::rgba_to_vec(pen_color);
        gdi_cmd.opcode_ = epoc::gdi_store_command_draw_rect;

        if (!epoc::is_display_mode_alpha(attached_window->display_mode())) {
            rect_draw_data.color_.w = 255;
        }
        
        attached_window->add_draw_command(gdi_cmd);
        context.complete(epoc::error_none);
    }

    void graphic_context::reset_context() {
        if (text_font) {
            text_font = nullptr;
        }

        fill_mode = brush_style::null;
        line_mode = pen_style::solid;

        pen_size = { 1, 1 };
        brush_color = 0xFFFFFFFF;
        pen_color = 0;

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
        context.complete(epoc::error_none);
    }

    void graphic_context::discard_font(service::ipc_context &context, ws_cmd &cmd) {
        if (text_font) {
            text_font = nullptr;
        }
    }

    void graphic_context::set_underline_style(service::ipc_context &context, ws_cmd &cmd) {
        // TODO: Implement set underline style
        context.complete(epoc::error_none);
    }

    void graphic_context::set_strikethrough_style(service::ipc_context &context, ws_cmd &cmd) {
        // TODO: Implement set strikethrough style
        context.complete(epoc::error_none);
    }
    
    void graphic_context::set_draw_mode(service::ipc_context &context, ws_cmd &cmd) {
        // Not easy to implement under hardware acceleration, ignore for now
        context.complete(epoc::error_none);
    }

    void graphic_context::draw_text(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_draw_text *info = reinterpret_cast<decltype(info)>(cmd.data_ptr);
        std::u16string text(reinterpret_cast<char16_t *>(reinterpret_cast<std::uint8_t *>(cmd.data_ptr) + sizeof(ws_cmd_draw_text)), info->length);

        do_command_draw_text(context, info->pos, info->pos, text,
            epoc::text_alignment::left, 0, 0, false);
    }

    void graphic_context::draw_box_text_optimised1(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_draw_box_text_optimised1 *info = reinterpret_cast<decltype(info)>(cmd.data_ptr);
        std::u16string text(reinterpret_cast<char16_t *>(reinterpret_cast<std::uint8_t *>(cmd.data_ptr) + sizeof(ws_cmd_draw_box_text_optimised1)), info->length);

        do_command_draw_text(context, info->left_top_pos, info->right_bottom_pos, text,
            epoc::text_alignment::left, info->baseline_offset, 0, true);
    }

    void graphic_context::draw_box_text_optimised2(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_draw_box_text_optimised2 *info = reinterpret_cast<decltype(info)>(cmd.data_ptr);
        std::u16string text(reinterpret_cast<char16_t *>(reinterpret_cast<std::uint8_t *>(cmd.data_ptr) + sizeof(ws_cmd_draw_box_text_optimised2)), info->length);

        do_command_draw_text(context, info->left_top_pos, info->right_bottom_pos, text,
            info->horiz, info->baseline_offset, info->left_mgr, true);
    }

    void graphic_context::set_clipping_rect(service::ipc_context &context, ws_cmd &cmd) {
        eka2l1::rect the_clip = *reinterpret_cast<eka2l1::rect *>(cmd.data_ptr);
        the_clip.transform_from_symbian_rectangle();

        clipping_rect = the_clip;
        do_submit_clipping();

        context.complete(epoc::error_none);
    }

    void graphic_context::cancel_clipping_rect(service::ipc_context &context, ws_cmd &cmd) {
        clipping_rect.make_empty();    
        do_submit_clipping();

        context.complete(epoc::error_none);    
    }

    void graphic_context::destroy(service::ipc_context &context, ws_cmd &cmd) {
        context.complete(epoc::error_none);
        client->delete_object(cmd.obj_handle);
    }

    void graphic_context::set_opaque(service::ipc_context &context, ws_cmd &cmd) {
        // TODO: Unstub!
        context.complete(epoc::error_none);
    }

    bool graphic_context::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        //LOG_TRACE(SERVICE_WINDOW, "Graphics context opcode {}", cmd.header.op);
        ws_graphics_context_opcode op = static_cast<decltype(op)>(cmd.header.op);

        using ws_graphics_context_op_handler = std::function<void(graphic_context *,
            service::ipc_context & ctx, ws_cmd & cmd)>;

        using ws_graphics_context_table_op = std::map<ws_graphics_context_opcode, std::tuple<ws_graphics_context_op_handler, bool, bool>>;

        // The order of the variables follow in this order.
        // opcode | (function pointer to implementation, will need to flush graphics, will end drawing)
        // If the function pointer to implementation is nullptr, it's silently ignored! However, if the function
        // is not presented in this list at all, a warning will be issued.
        static const ws_graphics_context_table_op v139u_opcode_handlers = {
            { ws_gc_u139_active, { &graphic_context::active, false, false } },
            { ws_gc_u139_set_clipping_rect, { &graphic_context::set_clipping_rect, false, false } },
            { ws_gc_u139_cancel_clipping_rect, { &graphic_context::cancel_clipping_rect, false, false } },
            { ws_gc_u139_set_brush_color, { &graphic_context::set_brush_color, false, false } },
            { ws_gc_u139_set_brush_style, { &graphic_context::set_brush_style, false, false } },
            { ws_gc_u139_set_pen_color, { &graphic_context::set_pen_color, false, false } },
            { ws_gc_u139_set_pen_style, { &graphic_context::set_pen_style, false, false } },
            { ws_gc_u139_set_pen_size, { &graphic_context::set_pen_size, false, false } },
            { ws_gc_u139_set_underline_style, { &graphic_context::set_underline_style, false, false } },
            { ws_gc_u139_set_strikethrough_style, { &graphic_context::set_strikethrough_style, false, false } },
            { ws_gc_u139_set_draw_mode, { &graphic_context::set_draw_mode, false, false } }, 
            { ws_gc_u139_deactive, { &graphic_context::deactive, false, false } },
            { ws_gc_u139_reset, { &graphic_context::reset, false, false } },
            { ws_gc_u139_use_font, { &graphic_context::use_font, false, false } },
            { ws_gc_u139_discard_font, { &graphic_context::discard_font, false, false } },
            { ws_gc_u139_draw_line, { &graphic_context::draw_line, true, false } },
            { ws_gc_u139_draw_rect, { &graphic_context::draw_rect, true, false } },
            { ws_gc_u139_clear, { &graphic_context::clear, true, false } },
            { ws_gc_u139_clear_rect, { &graphic_context::clear_rect, true, false } },
            { ws_gc_u139_draw_bitmap, { &graphic_context::draw_bitmap, true, false } },
            { ws_gc_u139_draw_bitmap2, { &graphic_context::draw_bitmap_2, true, false } },
            { ws_gc_u139_draw_bitmap3, { &graphic_context::draw_bitmap_3, true, false } },
            { ws_gc_u139_draw_text, { &graphic_context::draw_text, true, false } },
            { ws_gc_u139_draw_box_text_optimised1, { &graphic_context::draw_box_text_optimised1, true, false } },
            { ws_gc_u139_draw_box_text_optimised2, { &graphic_context::draw_box_text_optimised2, true, false } },
            { ws_gc_u139_gdi_blt2, { &graphic_context::gdi_blt2, true, false } },
            { ws_gc_u139_gdi_blt3, { &graphic_context::gdi_blt3, true, false } },
            { ws_gc_u139_gdi_ws_blt2, { &graphic_context::gdi_ws_blt2, true, false } },
            { ws_gc_u139_gdi_ws_blt3, { &graphic_context::gdi_ws_blt3, true, false } },
            { ws_gc_u139_gdi_blt_masked, { &graphic_context::gdi_blt_masked, true, false } },
            { ws_gc_u139_gdi_ws_blt_masked, { &graphic_context::gdi_ws_blt_masked, true, false } },
            { ws_gc_u139_plot, { &graphic_context::plot, true, false } },
            { ws_gc_u139_set_faded, { nullptr, true, false } },
            { ws_gc_u139_set_fade_params, { nullptr, true, false } },
            { ws_gc_u139_free, { &graphic_context::destroy, true, true } }
        };

        static const ws_graphics_context_table_op v151u_m1_opcode_handlers = {
            { ws_gc_u151m1_active, { &graphic_context::active, false, false } },
            { ws_gc_u151m1_set_clipping_rect, { &graphic_context::set_clipping_rect, false, false } },
            { ws_gc_u151m1_cancel_clipping_rect, { &graphic_context::cancel_clipping_rect, false, false } },
            { ws_gc_u151m1_set_brush_color, { &graphic_context::set_brush_color, false, false } },
            { ws_gc_u151m1_set_brush_style, { &graphic_context::set_brush_style, false, false } },
            { ws_gc_u151m1_set_pen_color, { &graphic_context::set_pen_color, false, false } },
            { ws_gc_u151m1_set_pen_style, { &graphic_context::set_pen_style, false, false } },
            { ws_gc_u151m1_set_pen_size, { &graphic_context::set_pen_size, false, false } },
            { ws_gc_u151m1_deactive, { &graphic_context::deactive, false, false } },
            { ws_gc_u151m1_reset, { &graphic_context::reset, false, false } },
            { ws_gc_u151m1_use_font, { &graphic_context::use_font, false, false } },
            { ws_gc_u151m1_discard_font, { &graphic_context::discard_font, false, false } },
            { ws_gc_u151m1_draw_line, { &graphic_context::draw_line, true, false } },
            { ws_gc_u151m1_draw_rect, { &graphic_context::draw_rect, true, false } },
            { ws_gc_u151m1_clear, { &graphic_context::clear, true, false } },
            { ws_gc_u151m1_clear_rect, { &graphic_context::clear_rect, true, false } },
            { ws_gc_u151m1_draw_bitmap, { &graphic_context::draw_bitmap, true, false } },
            { ws_gc_u151m1_draw_bitmap2, { &graphic_context::draw_bitmap_2, true, false } },
            { ws_gc_u151m1_draw_bitmap3, { &graphic_context::draw_bitmap_3, true, false } },
            { ws_gc_u151m1_draw_text, { &graphic_context::draw_text, true, false } },
            { ws_gc_u151m1_draw_box_text_optimised1, { &graphic_context::draw_box_text_optimised1, true, false } },
            { ws_gc_u151m1_draw_box_text_optimised2, { &graphic_context::draw_box_text_optimised2, true, false } },
            { ws_gc_u151m1_gdi_blt2, { &graphic_context::gdi_blt2, true, false } },
            { ws_gc_u151m1_gdi_blt3, { &graphic_context::gdi_blt3, true, false } },
            { ws_gc_u151m1_gdi_ws_blt2, { &graphic_context::gdi_ws_blt2, true, false } },
            { ws_gc_u151m1_gdi_ws_blt3, { &graphic_context::gdi_ws_blt3, true, false } },
            { ws_gc_u151m1_gdi_blt_masked, { &graphic_context::gdi_blt_masked, true, false } },
            { ws_gc_u151m1_gdi_ws_blt_masked, { &graphic_context::gdi_ws_blt_masked, true, false } },
            { ws_gc_u151m1_plot, { &graphic_context::plot, true, false } },
            { ws_gc_u151m1_set_faded, { nullptr, true, false } },
            { ws_gc_u151m1_set_fade_params, { nullptr, true, false } },
            { ws_gc_u151m1_free, { &graphic_context::destroy, true, true } }
        };

        static const ws_graphics_context_table_op v151u_m2_opcode_handlers = {
            { ws_gc_u151m2_active, { &graphic_context::active, false, false } },
            { ws_gc_u151m2_set_clipping_rect, { &graphic_context::set_clipping_rect, false, false } },
            { ws_gc_u151m2_cancel_clipping_rect, { &graphic_context::cancel_clipping_rect, false, false } },
            { ws_gc_u151m2_set_brush_color, { &graphic_context::set_brush_color, false, false } },
            { ws_gc_u151m2_set_brush_style, { &graphic_context::set_brush_style, false, false } },
            { ws_gc_u151m2_set_pen_color, { &graphic_context::set_pen_color, false, false } },
            { ws_gc_u151m2_set_pen_style, { &graphic_context::set_pen_style, false, false } },
            { ws_gc_u151m2_set_pen_size, { &graphic_context::set_pen_size, false, false } },
            { ws_gc_u151m2_set_underline_style, { &graphic_context::set_underline_style, false, false } },
            { ws_gc_u151m2_set_strikethrough_style, { &graphic_context::set_strikethrough_style, false, false } },
            { ws_gc_u151m2_set_draw_mode, { &graphic_context::set_draw_mode, false, false } }, 
            { ws_gc_u151m2_deactive, { &graphic_context::deactive, false, false } },
            { ws_gc_u151m2_reset, { &graphic_context::reset, false, false } },
            { ws_gc_u151m2_use_font, { &graphic_context::use_font, false, false } },
            { ws_gc_u151m2_discard_font, { &graphic_context::discard_font, false, false } },
            { ws_gc_u151m2_draw_line, { &graphic_context::draw_line, true, false } },
            { ws_gc_u151m2_draw_rect, { &graphic_context::draw_rect, true, false } },
            { ws_gc_u151m2_clear, { &graphic_context::clear, true, false } },
            { ws_gc_u151m2_clear_rect, { &graphic_context::clear_rect, true, false } },
            { ws_gc_u151m2_draw_bitmap, { &graphic_context::draw_bitmap, true, false } },
            { ws_gc_u151m2_draw_bitmap2, { &graphic_context::draw_bitmap_2, true, false } },
            { ws_gc_u151m2_draw_bitmap3, { &graphic_context::draw_bitmap_3, true, false } },
            { ws_gc_u151m2_ws_draw_bitmap_masked, { &graphic_context::ws_draw_bitmap_masked, true, false } },
            { ws_gc_u151m2_draw_text, { &graphic_context::draw_text, true, false } },
            { ws_gc_u151m2_draw_box_text_optimised1, { &graphic_context::draw_box_text_optimised1, true, false } },
            { ws_gc_u151m2_draw_box_text_optimised2, { &graphic_context::draw_box_text_optimised2, true, false } },
            { ws_gc_u151m2_gdi_blt2, { &graphic_context::gdi_blt2, true, false } },
            { ws_gc_u151m2_gdi_blt3, { &graphic_context::gdi_blt3, true, false } },
            { ws_gc_u151m2_gdi_ws_blt2, { &graphic_context::gdi_ws_blt2, true, false } },
            { ws_gc_u151m2_gdi_ws_blt3, { &graphic_context::gdi_ws_blt3, true, false } },
            { ws_gc_u151m2_gdi_blt_masked, { &graphic_context::gdi_blt_masked, true, false } },
            { ws_gc_u151m2_gdi_ws_blt_masked, { &graphic_context::gdi_ws_blt_masked, true, false } },
            { ws_gc_u151m2_plot, { &graphic_context::plot, true, false } },
            { ws_gc_u151m2_set_faded, { nullptr, true, false } },
            { ws_gc_u151m2_set_fade_params, { nullptr, true, false } },
            { ws_gc_u151m2_set_opaque, { &graphic_context::set_opaque, true, false } },
            { ws_gc_u151m2_free, { &graphic_context::destroy, true, true } }
        };

        static const ws_graphics_context_table_op curr_opcode_handlers = {
            { ws_gc_curr_active, { &graphic_context::active, false, false } },
            { ws_gc_curr_set_clipping_rect, { &graphic_context::set_clipping_rect, false, false } },
            { ws_gc_curr_cancel_clipping_rect, { &graphic_context::cancel_clipping_rect, false, false } },
            { ws_gc_curr_set_brush_color, { &graphic_context::set_brush_color, false, false } },
            { ws_gc_curr_set_brush_style, { &graphic_context::set_brush_style, false, false } },
            { ws_gc_curr_set_pen_color, { &graphic_context::set_pen_color, false, false } },
            { ws_gc_curr_set_pen_style, { &graphic_context::set_pen_style, false, false } },
            { ws_gc_curr_set_pen_size, { &graphic_context::set_pen_size, false, false } },
            { ws_gc_curr_set_underline_style, { &graphic_context::set_underline_style, false, false } },
            { ws_gc_curr_set_strikethrough_style, { &graphic_context::set_strikethrough_style, false, false } },
            { ws_gc_curr_set_draw_mode, { &graphic_context::set_draw_mode, false, false } }, 
            { ws_gc_curr_deactive, { &graphic_context::deactive, false, false } },
            { ws_gc_curr_reset, { &graphic_context::reset, false, false } },
            { ws_gc_curr_use_font, { &graphic_context::use_font, false, false } },
            { ws_gc_curr_discard_font, { &graphic_context::discard_font, false, false } },
            { ws_gc_curr_draw_line, { &graphic_context::draw_line, true, false } },
            { ws_gc_curr_draw_rect, { &graphic_context::draw_rect, true, false } },
            { ws_gc_curr_clear, { &graphic_context::clear, true, false } },
            { ws_gc_curr_clear_rect, { &graphic_context::clear_rect, true, false } },
            { ws_gc_curr_draw_bitmap, { &graphic_context::draw_bitmap, true, false } },
            { ws_gc_curr_draw_bitmap2, { &graphic_context::draw_bitmap_2, true, false } },
            { ws_gc_curr_draw_bitmap3, { &graphic_context::draw_bitmap_3, true, false } },
            { ws_gc_curr_ws_draw_bitmap_masked, { &graphic_context::ws_draw_bitmap_masked, true, false } },
            { ws_gc_curr_draw_text, { &graphic_context::draw_text, true, false } },
            { ws_gc_curr_draw_box_text_optimised1, { &graphic_context::draw_box_text_optimised1, true, false } },
            { ws_gc_curr_draw_box_text_optimised2, { &graphic_context::draw_box_text_optimised2, true, false } },
            { ws_gc_curr_gdi_blt2, { &graphic_context::gdi_blt2, true, false } },
            { ws_gc_curr_gdi_blt3, { &graphic_context::gdi_blt3, true, false } },
            { ws_gc_curr_gdi_ws_blt2, { &graphic_context::gdi_ws_blt2, true, false } },
            { ws_gc_curr_gdi_ws_blt3, { &graphic_context::gdi_ws_blt3, true, false } },
            { ws_gc_curr_gdi_blt_masked, { &graphic_context::gdi_blt_masked, true, false } },
            { ws_gc_curr_gdi_ws_blt_masked, { &graphic_context::gdi_ws_blt_masked, true, false } },
            { ws_gc_curr_plot, { &graphic_context::plot, true, false } },
            { ws_gc_curr_set_faded, { nullptr, true, false } },
            { ws_gc_curr_set_fade_params, { nullptr, true, false } },
            { ws_gc_curr_set_opaque, { &graphic_context::set_opaque, true, false } },
            { ws_gc_curr_free, { &graphic_context::destroy, true, true } }
        };

        epoc::version cli_ver = client->client_version();
        ws_graphics_context_op_handler handler = nullptr;

        bool need_to_set_flushed = false;
        bool need_quit = false;

#define FIND_OPCODE(op, table)                                                               \
    auto result = table.find(op);                                                            \
    if (result == table.end()) {                                                             \
        LOG_WARN(SERVICE_WINDOW, "Unimplemented graphics context opcode {}", cmd.header.op); \
        return false;                                                                        \
    }                                                                                        \
    if (!std::get<0>(result->second)) {                                                    \
        return false;                                                                        \
    }                                                                                        \
    handler = std::get<0>(result->second);                                                   \
    need_to_set_flushed = std::get<1>(result->second);                                       \
    need_quit = std::get<2>(result->second);

        kernel_system *kern = client->get_ws().get_kernel_system();

        if (cli_ver.major == 1 && cli_ver.minor == 0) {
            if (cli_ver.build <= WS_OLDARCH_VER) {
                FIND_OPCODE(op, v139u_opcode_handlers)
            } else if (cli_ver.build <= WS_NEWARCH_VER) {
                if (kern->get_epoc_version() <= epocver::epoc80) {
                    FIND_OPCODE(op, v139u_opcode_handlers)
                } else if (kern->get_epoc_version() <= epocver::epoc81b) {
                    FIND_OPCODE(op, v151u_m1_opcode_handlers)
                } else if (kern->get_epoc_version() <= epocver::epoc94) {
                    FIND_OPCODE(op, v151u_m2_opcode_handlers)
                } else {
                    FIND_OPCODE(op, curr_opcode_handlers)
                }
            } else {
                // Execute table 3
                FIND_OPCODE(op, curr_opcode_handlers)
            }
        }

        if (need_to_set_flushed) {
            flushed = false;
        }

        handler(this, ctx, cmd);
        return need_quit;
    }

    graphic_context::graphic_context(window_server_client_ptr client, epoc::window *attach_win)
        : window_client_obj(client, nullptr)
        , attached_window(reinterpret_cast<epoc::canvas_base *>(attach_win))
        , text_font(nullptr)
        , fill_mode(brush_style::null)
        , line_mode(pen_style::solid)
        , brush_color(0xFFFFFFFF)
        , pen_color(0)
        , pen_size(1, 1) {
    }
    
    graphic_context::~graphic_context() {
        context_attach_link.deque();
    }
}
