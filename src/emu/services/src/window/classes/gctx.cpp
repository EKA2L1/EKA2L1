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
#include <services/window/window.h>

#include <system/epoc.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/rgb.h>

#include <utils/err.h>

namespace eka2l1::epoc {
    bool graphic_context::no_building() const {
        return !attached_window || (attached_window->size == eka2l1::vec2(0, 0));
    }

    void graphic_context::active(service::ipc_context &context, ws_cmd cmd) {
        const std::uint32_t window_to_attach_handle = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);
        attached_window = reinterpret_cast<epoc::window_user *>(client->get_object(window_to_attach_handle));

        // Attach context with window
        attached_window->attached_contexts.push(&context_attach_link);

        // Afaik that the pointer to CWsScreenDevice is internal, so not so scared of general users touching
        // this.
        context.complete(attached_window->scr->number);

        if (no_building()) {
            cmd_list = nullptr;
            cmd_builder = nullptr;

            return;
        }

        drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();

        // Make new command list
        if (!cmd_list) {
            cmd_list = drv->new_command_list();
            cmd_builder = drv->new_command_builder(cmd_list.get());
        }

        // Add first command list, binding our window bitmap
        if (attached_window->driver_win_id == 0) {
            kernel_system *kern = context.sys->get_kernel_system();

            // We already completed our request, so it's no fear to unlock the kernel now!
            kern->unlock();

            attached_window->driver_win_id = drivers::create_bitmap(drv, attached_window->size, 32);
            attached_window->resize_needed = false;

            kern->lock();
        }

        recording = true;

        eka2l1::rect viewport;
        viewport.top = { 0, 0 };
        viewport.size = attached_window->size;

        if (attached_window->resize_needed) {
            // Try to resize our bitmap. My NVIDIA did forgive me if texture has same spec
            // as before, but not Intel... Note: NVIDIA also
            cmd_builder->resize_bitmap(attached_window->driver_win_id, attached_window->size);
            attached_window->resize_needed = false;
        }

        cmd_builder->bind_bitmap(attached_window->driver_win_id);
        cmd_builder->set_depth(false);

        cmd_builder->set_viewport(viewport);
        cmd_builder->clear({ 0, 0, 0, 0 }, drivers::draw_buffer_bit_stencil_buffer);

        // Reset clipping. This is not mentioned in doc but is in official source code.
        // See gc.cpp file. Opcode EWsGcOpActivate
        clipping_rect.make_empty();
        clipping_region.make_empty();

        do_submit_clipping();
    }

    void graphic_context::do_command_draw_bitmap(service::ipc_context &ctx, drivers::handle h, const eka2l1::rect &source_rect, const eka2l1::rect &dest_rect) {
        if (cmd_builder)
            cmd_builder->draw_bitmap(h, 0, dest_rect, source_rect, eka2l1::vec2(0, 0), 0.0f, 0);

        ctx.complete(epoc::error_none);
    }

    void graphic_context::do_command_draw_text(service::ipc_context &ctx, eka2l1::vec2 top_left,
        eka2l1::vec2 bottom_right, const std::u16string &text, epoc::text_alignment align,
        const int baseline_offset, const int margin, const bool fill_surrounding) {
        // Complete it first, cause we gonna open up the kernel
        ctx.complete(epoc::error_none);

        if (cmd_builder) {
            eka2l1::rect area(top_left, bottom_right - top_left);
            if (fill_surrounding) {
                // The effective box colour depends on the drawing mode. As the document says
                if (do_command_set_brush_color()) {
                    cmd_builder->draw_rectangle(area);
                }
            }

            // TODO: Pen outline >_<
            eka2l1::vecx<std::uint8_t, 4> color;
            color = common::rgba_to_vec(pen_color);
            cmd_builder->set_brush_color({ color[0], color[1], color[2] });

            // Add the baseline offset. Where text will sit on.
            area.top.y += baseline_offset;

            if (align == epoc::text_alignment::right) {
                area.top.x -= margin;
            } else {
                area.top.x += margin;
            }

            kernel_system *kern = client->get_ws().get_kernel_system();

            kern->unlock();
            text_font->atlas.draw_text(text, area, align, client->get_ws().get_graphics_driver(),
                cmd_builder.get());

            kern->lock();
        }
    }

    bool graphic_context::do_command_set_brush_color() {
        if (!cmd_builder) {
            return false;
        }

        eka2l1::vecx<std::uint8_t, 4> color = common::rgba_to_vec(brush_color);

        // Don't bother even sending any draw command
        switch (fill_mode) {
        case brush_style::null:
            return false;

        case brush_style::solid:
            if (epoc::is_display_mode_alpha(attached_window->display_mode())) {
                if (color[3] == 0) {
                    // Nothing, dont draw
                    return false;
                }

                cmd_builder->set_brush_color_detail({ color[0], color[1], color[2], color[3] });
            } else {
                cmd_builder->set_brush_color({ color[0], color[1], color[2] });
            }

            break;

        default:
            LOG_WARN(SERVICE_WINDOW, "Unhandled brush style {}", static_cast<std::int32_t>(fill_mode));
            return false;
        }

        return true;
    }

    bool graphic_context::do_command_set_pen_color() {
        eka2l1::vecx<std::uint8_t, 4> color = common::rgba_to_vec(pen_color);

        // Don't bother even sending any draw command
        switch (line_mode) {
        case pen_style::null:
            return false;

        case pen_style::solid:
            if (epoc::is_display_mode_alpha(attached_window->display_mode())) {
                if (color[3] == 0) {
                    // Nothing, dont draw
                    return false;
                }

                cmd_builder->set_brush_color_detail({ color[0], color[1], color[2], color[3] });
            } else {
                cmd_builder->set_brush_color({ color[0], color[1], color[2] });
            }

            break;

        default:
            LOG_WARN(SERVICE_WINDOW, "Unhandled pen style {}", static_cast<std::int32_t>(fill_mode));
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
                // Developement document says that when not being redrawn, drawing is clipped to non-invalid part.
                // But so far, I have not been able to see any open source code points to that being true. Even simple test can prove that's false.
                // So for now, we let drawing happens on the window with no restrictions. Invalid region will still be invalidated.
                cmd_builder->set_clipping(false);
                cmd_builder->set_stencil(false);

                return;
            }
        } else {
            if (attached_window->flags & epoc::window_user::flags_in_redraw) {
                clip_region_temp->add_rect(attached_window->redraw_rect_curr);
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
        if (!flushed) {
            drivers::graphics_driver *driver = client->get_ws().get_graphics_driver();

            // Unbind current bitmap
            cmd_builder->bind_bitmap(0);

            cmd_builder->set_clipping(false);
            cmd_builder->set_stencil(false);

            driver->submit_command_list(*cmd_list);

            // Renew this so that the graphic context can continue
            cmd_list = driver->new_command_list();
            cmd_builder = driver->new_command_builder(cmd_list.get());
            
            cmd_builder->bind_bitmap(attached_window->driver_win_id);
            cmd_builder->set_depth(false);

            eka2l1::rect viewport;
            viewport.top = { 0, 0 };
            viewport.size = attached_window->size;

            cmd_builder->set_viewport(viewport);

            do_submit_clipping();
        }
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
        if (cmd_list && !flushed) {
            flush_queue_to_driver();

            // Content of the window changed, so call the handler
            attached_window->take_action_on_change(rq);
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

    drivers::handle graphic_context::handle_from_bitwise_bitmap(epoc::bitwise_bitmap *bmp) {
        drivers::graphics_driver *driver = client->get_ws().get_graphics_driver();
        epoc::bitmap_cache *cacher = client->get_ws().get_bitmap_cache();

        kernel_system *kern = client->get_ws().get_kernel_system();

        // This call maybe unsafe, as someone may delete our thread before request complete! :((
        // TODO: Safer condition for unlocking.
        kern->unlock();
        drivers::handle h = cacher->add_or_get(driver, cmd_builder.get(), bmp);
        kern->lock();

        return h;
    }

    void graphic_context::draw_bitmap(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_draw_bitmap *bitmap_cmd = reinterpret_cast<ws_cmd_draw_bitmap *>(cmd.data_ptr);
        epoc::bitwise_bitmap *bw_bmp = client->get_ws().get_bitmap(bitmap_cmd->handle);

        if (!bw_bmp) {
            context.complete(epoc::error_argument);
            return;
        }

        drivers::handle bmp_driver_handle = handle_from_bitwise_bitmap(bw_bmp);
        do_command_draw_bitmap(context, bmp_driver_handle, rect({ 0, 0 }, bw_bmp->header_.size_pixels), rect(bitmap_cmd->pos, { 0, 0 }));
    }

    void graphic_context::draw_bitmap_2(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_draw_bitmap2 *bitmap_cmd = reinterpret_cast<ws_cmd_draw_bitmap2 *>(cmd.data_ptr);
        epoc::bitwise_bitmap *bw_bmp = client->get_ws().get_bitmap(bitmap_cmd->handle);

        if (!bw_bmp) {
            context.complete(epoc::error_argument);
            return;
        }

        drivers::handle bmp_driver_handle = handle_from_bitwise_bitmap(bw_bmp);

        eka2l1::rect dest_rect = bitmap_cmd->dest_rect;
        dest_rect.transform_from_symbian_rectangle();

        do_command_draw_bitmap(context, bmp_driver_handle, rect({ 0, 0 }, bw_bmp->header_.size_pixels), dest_rect);
    }

    void graphic_context::draw_bitmap_3(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_draw_bitmap3 *bitmap_cmd = reinterpret_cast<ws_cmd_draw_bitmap3 *>(cmd.data_ptr);
        epoc::bitwise_bitmap *bw_bmp = client->get_ws().get_bitmap(bitmap_cmd->handle);

        if (!bw_bmp) {
            context.complete(epoc::error_argument);
            return;
        }

        drivers::handle bmp_driver_handle = handle_from_bitwise_bitmap(bw_bmp);

        eka2l1::rect source_rect = bitmap_cmd->source_rect;
        eka2l1::rect dest_rect = bitmap_cmd->dest_rect;

        source_rect.transform_from_symbian_rectangle();
        dest_rect.transform_from_symbian_rectangle();

        do_command_draw_bitmap(context, bmp_driver_handle, source_rect, dest_rect);
    }

    void graphic_context::draw_mask_impl(epoc::bitwise_bitmap *source_bitmap, epoc::bitwise_bitmap *mask_bitmap, const eka2l1::rect &dest_rect, const eka2l1::rect &source_rect, const bool invert_mask) {
        drivers::handle bmp_driver_handle = handle_from_bitwise_bitmap(source_bitmap);
        drivers::handle bmp_mask_driver_handle = handle_from_bitwise_bitmap(mask_bitmap);

        std::uint32_t flags = 0;
        const bool alpha_blending = (mask_bitmap->settings_.current_display_mode() == epoc::display_mode::gray256)
            || (epoc::is_display_mode_alpha(mask_bitmap->settings_.current_display_mode()));

        if (invert_mask && !alpha_blending) {
            flags |= drivers::bitmap_draw_flag_invert_mask;
        }

        if (!alpha_blending) {
            flags |= drivers::bitmap_draw_flag_flat_blending;
        }

        cmd_builder->set_blend_mode(true);
        
        // For non alpha blending we always want to take color buffer's alpha.
        cmd_builder->blend_formula(drivers::blend_equation::add, drivers::blend_equation::add,
            drivers::blend_factor::frag_out_alpha, drivers::blend_factor::one_minus_frag_out_alpha,
            (alpha_blending ? drivers::blend_factor::frag_out_alpha : drivers::blend_factor::one),
            (alpha_blending ? drivers::blend_factor::one_minus_frag_out_alpha : drivers::blend_factor::one));

        bool swizzle_alteration = false;
        if (!alpha_blending && !epoc::is_display_mode_alpha(mask_bitmap->settings_.current_display_mode())) {
            swizzle_alteration = true;
            cmd_builder->set_swizzle(bmp_mask_driver_handle, drivers::channel_swizzle::red, drivers::channel_swizzle::green,
                drivers::channel_swizzle::blue, drivers::channel_swizzle::red);
        }

        cmd_builder->draw_bitmap(bmp_driver_handle, bmp_mask_driver_handle, dest_rect, source_rect, eka2l1::vec2(0, 0),
            0.0f, flags);
        cmd_builder->set_blend_mode(false);

        if (swizzle_alteration) {
            cmd_builder->set_swizzle(bmp_mask_driver_handle, drivers::channel_swizzle::red, drivers::channel_swizzle::green,
                drivers::channel_swizzle::blue, drivers::channel_swizzle::alpha);
        }
    }

    void graphic_context::ws_draw_bitmap_masked(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_draw_ws_bitmap_masked *blt_cmd = reinterpret_cast<ws_cmd_draw_ws_bitmap_masked *>(cmd.data_ptr);

        epoc::wsbitmap *myside_source_bmp = reinterpret_cast<epoc::wsbitmap*>(client->get_object(blt_cmd->source_handle));
        epoc::wsbitmap *myside_mask_bmp = reinterpret_cast<epoc::wsbitmap*>(client->get_object(blt_cmd->mask_handle));

        if (!myside_source_bmp || !myside_mask_bmp) {
            context.complete(epoc::error_bad_handle);
            return;
        }

        eka2l1::rect source_rect = blt_cmd->source_rect;
        eka2l1::rect dest_rect = blt_cmd->dest_rect;

        source_rect.transform_from_symbian_rectangle();
        dest_rect.transform_from_symbian_rectangle();

        draw_mask_impl(myside_source_bmp->bitmap_, myside_mask_bmp->bitmap_, dest_rect, source_rect, static_cast<bool>(blt_cmd->invert_mask));
        context.complete(epoc::error_none);
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

        draw_mask_impl(bmp, masked, dest_rect, source_rect, blt_cmd->invert_mask);
        context.complete(epoc::error_none);
    }

    void graphic_context::gdi_ws_blt_masked(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_gdi_blt_masked *blt_cmd = reinterpret_cast<ws_cmd_gdi_blt_masked *>(cmd.data_ptr);

        epoc::wsbitmap *myside_source_bmp = reinterpret_cast<epoc::wsbitmap*>(client->get_object(blt_cmd->source_handle));
        epoc::wsbitmap *myside_mask_bmp = reinterpret_cast<epoc::wsbitmap*>(client->get_object(blt_cmd->mask_handle));

        if (!myside_source_bmp || !myside_mask_bmp) {
            context.complete(epoc::error_bad_handle);
            return;
        }

        eka2l1::rect source_rect = blt_cmd->source_rect;
        source_rect.transform_from_symbian_rectangle();

        eka2l1::rect dest_rect;
        dest_rect.size = source_rect.size;
        dest_rect.top = blt_cmd->pos;

        draw_mask_impl(myside_source_bmp->bitmap_, myside_mask_bmp->bitmap_, dest_rect, source_rect, static_cast<bool>(blt_cmd->invert_mask));
        context.complete(epoc::error_none);
    }

    void graphic_context::gdi_blt_impl(service::ipc_context &context, ws_cmd &cmd, const int ver, const bool ws) {
        ws_cmd_gdi_blt3 *blt_cmd = reinterpret_cast<ws_cmd_gdi_blt3 *>(cmd.data_ptr);

        // Try to get the bitmap
        epoc::bitwise_bitmap *bmp = nullptr;
        
        if (ws) {
            epoc::wsbitmap *myside_bmp = reinterpret_cast<epoc::wsbitmap*>(client->get_object(blt_cmd->handle));

            if (!myside_bmp) {
                context.complete(epoc::error_bad_handle);
                return;
            }

            bmp = myside_bmp->bitmap_;
        } else {
            bmp = client->get_ws().get_bitmap(blt_cmd->handle);
        }

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
        eka2l1::rect the_clip = *reinterpret_cast<eka2l1::rect*>(cmd.data_ptr);
        the_clip.transform_from_symbian_rectangle();

        clipping_rect = the_clip;
        do_submit_clipping();

        context.complete(epoc::error_none);
    }

    void graphic_context::free(service::ipc_context &context, ws_cmd &cmd) {
        on_command_batch_done(context);

        context.complete(epoc::error_none);
        client->delete_object(cmd.obj_handle);
    }

    bool graphic_context::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        //LOG_TRACE(SERVICE_WINDOW, "Graphics context opcode {}", cmd.header.op);
        ws_graphics_context_opcode op = static_cast<decltype(op)>(cmd.header.op);

        // General rules: Stub to err_none = nullptr, implement = function pointer
        //                Do nothing = add nothing
        using ws_graphics_context_op_handler = std::function<void(graphic_context *,
            service::ipc_context & ctx, ws_cmd & cmd)>;

        using ws_graphics_context_table_op = std::map<ws_graphics_context_opcode, std::tuple<ws_graphics_context_op_handler, bool, bool>>;

        static const ws_graphics_context_table_op v139u_opcode_handlers = {
            { ws_gc_u139_active, { &graphic_context::active , false, false } },
            { ws_gc_u139_set_clipping_rect, { &graphic_context::set_clipping_rect , false, false } },
            { ws_gc_u139_set_brush_color, { &graphic_context::set_brush_color , false, false } },
            { ws_gc_u139_set_brush_style, { &graphic_context::set_brush_style , false, false } },
            { ws_gc_u139_set_pen_color, { &graphic_context::set_pen_color , false, false } },
            { ws_gc_u139_set_pen_style, { &graphic_context::set_pen_style , false, false } },
            { ws_gc_u139_set_pen_size, { &graphic_context::set_pen_size , false, false } },
            { ws_gc_u139_deactive, { &graphic_context::deactive , false, false } },
            { ws_gc_u139_reset, { &graphic_context::reset , false, false } },
            { ws_gc_u139_use_font, { &graphic_context::use_font , false, false } },
            { ws_gc_u139_discard_font, { &graphic_context::discard_font , false, false } },
            { ws_gc_u139_draw_line, { &graphic_context::draw_line , true, false } },
            { ws_gc_u139_draw_rect, { &graphic_context::draw_rect , true, false } },
            { ws_gc_u139_clear, { &graphic_context::clear , true, false } },
            { ws_gc_u139_clear_rect, { &graphic_context::clear_rect , true, false } },
            { ws_gc_u139_draw_bitmap, { &graphic_context::draw_bitmap , true, false } },
            { ws_gc_u139_draw_bitmap2, { &graphic_context::draw_bitmap_2 , true, false } },
            { ws_gc_u139_draw_bitmap3, { &graphic_context::draw_bitmap_3 , true, false } },
            { ws_gc_u139_draw_text, { &graphic_context::draw_text , true, false } },
            { ws_gc_u139_draw_box_text_optimised1, { &graphic_context::draw_box_text_optimised1 , true, false } },
            { ws_gc_u139_draw_box_text_optimised2, { &graphic_context::draw_box_text_optimised2 , true, false } },
            { ws_gc_u139_gdi_blt2, { &graphic_context::gdi_blt2 , true, false } },
            { ws_gc_u139_gdi_blt3, { &graphic_context::gdi_blt3 , true, false } },
            { ws_gc_u139_gdi_ws_blt2, { &graphic_context::gdi_ws_blt2 , true, false } },
            { ws_gc_u139_gdi_ws_blt3, { &graphic_context::gdi_ws_blt3 , true, false } },
            { ws_gc_u139_gdi_blt_masked, { &graphic_context::gdi_blt_masked , true, false } },
            { ws_gc_u139_gdi_ws_blt_masked, { &graphic_context::gdi_ws_blt_masked , true, false } },
            { ws_gc_u139_free, { &graphic_context::free , true, true } }
        };

        static const ws_graphics_context_table_op v151u_m1_opcode_handlers = {
            { ws_gc_u151m1_active, { &graphic_context::active , false, false } },
            { ws_gc_u151m1_set_clipping_rect, { &graphic_context::set_clipping_rect , false, false } },
            { ws_gc_u151m1_set_brush_color, { &graphic_context::set_brush_color , false, false } },
            { ws_gc_u151m1_set_brush_style, { &graphic_context::set_brush_style , false, false } },
            { ws_gc_u151m1_set_pen_color, { &graphic_context::set_pen_color , false, false } },
            { ws_gc_u151m1_set_pen_style, { &graphic_context::set_pen_style , false, false } },
            { ws_gc_u151m1_set_pen_size, { &graphic_context::set_pen_size , false, false } },
            { ws_gc_u151m1_deactive, { &graphic_context::deactive , false, false } },
            { ws_gc_u151m1_reset, { &graphic_context::reset , false, false } },
            { ws_gc_u151m1_use_font, { &graphic_context::use_font , false, false } },
            { ws_gc_u151m1_discard_font, { &graphic_context::discard_font , false, false } },
            { ws_gc_u151m1_draw_line, { &graphic_context::draw_line , true, false } },
            { ws_gc_u151m1_draw_rect, { &graphic_context::draw_rect , true, false } },
            { ws_gc_u151m1_clear, { &graphic_context::clear , true, false } },
            { ws_gc_u151m1_clear_rect, { &graphic_context::clear_rect , true, false } },
            { ws_gc_u151m1_draw_bitmap, { &graphic_context::draw_bitmap , true, false } },
            { ws_gc_u151m1_draw_bitmap2, { &graphic_context::draw_bitmap_2 , true, false } },
            { ws_gc_u151m1_draw_bitmap3, { &graphic_context::draw_bitmap_3 , true, false } },
            { ws_gc_u151m1_draw_text, { &graphic_context::draw_text , true, false } },
            { ws_gc_u151m1_draw_box_text_optimised1, { &graphic_context::draw_box_text_optimised1 , true, false } },
            { ws_gc_u151m1_draw_box_text_optimised2, { &graphic_context::draw_box_text_optimised2 , true, false } },
            { ws_gc_u151m1_gdi_blt2, { &graphic_context::gdi_blt2 , true, false } },
            { ws_gc_u151m1_gdi_blt3, { &graphic_context::gdi_blt3 , true, false } },
            { ws_gc_u151m1_gdi_ws_blt2, { &graphic_context::gdi_ws_blt2 , true, false } },
            { ws_gc_u151m1_gdi_ws_blt3, { &graphic_context::gdi_ws_blt3 , true, false } },
            { ws_gc_u151m1_gdi_blt_masked, { &graphic_context::gdi_blt_masked , true, false } },
            { ws_gc_u151m1_gdi_ws_blt_masked, { &graphic_context::gdi_ws_blt_masked , true, false } },
            { ws_gc_u151m1_free, { &graphic_context::free , true, true } }
        };

        static const ws_graphics_context_table_op v151u_m2_opcode_handlers = {
            { ws_gc_u151m2_active, { &graphic_context::active , false, false } },
            { ws_gc_u151m2_set_clipping_rect, { &graphic_context::set_clipping_rect , false, false } },
            { ws_gc_u151m2_set_brush_color, { &graphic_context::set_brush_color , false, false } },
            { ws_gc_u151m2_set_brush_style, { &graphic_context::set_brush_style , false, false } },
            { ws_gc_u151m2_set_pen_color, { &graphic_context::set_pen_color , false, false } },
            { ws_gc_u151m2_set_pen_style, { &graphic_context::set_pen_style , false, false } },
            { ws_gc_u151m2_set_pen_size, { &graphic_context::set_pen_size , false, false } },
            { ws_gc_u151m2_deactive, { &graphic_context::deactive , false, false } },
            { ws_gc_u151m2_reset, { &graphic_context::reset , false, false } },
            { ws_gc_u151m2_use_font, { &graphic_context::use_font , false, false } },
            { ws_gc_u151m2_discard_font, { &graphic_context::discard_font , false, false } },
            { ws_gc_u151m2_draw_line, { &graphic_context::draw_line , true, false } },
            { ws_gc_u151m2_draw_rect, { &graphic_context::draw_rect , true, false } },
            { ws_gc_u151m2_clear, { &graphic_context::clear , true, false } },
            { ws_gc_u151m2_clear_rect, { &graphic_context::clear_rect , true, false } },
            { ws_gc_u151m2_draw_bitmap, { &graphic_context::draw_bitmap , true, false } },
            { ws_gc_u151m2_draw_bitmap2, { &graphic_context::draw_bitmap_2 , true, false } },
            { ws_gc_u151m2_draw_bitmap3, { &graphic_context::draw_bitmap_3 , true, false } },
            { ws_gc_u151m2_ws_draw_bitmap_masked, { &graphic_context::ws_draw_bitmap_masked , true, false } },
            { ws_gc_u151m2_draw_text, { &graphic_context::draw_text , true, false } },
            { ws_gc_u151m2_draw_box_text_optimised1, { &graphic_context::draw_box_text_optimised1 , true, false } },
            { ws_gc_u151m2_draw_box_text_optimised2, { &graphic_context::draw_box_text_optimised2 , true, false } },
            { ws_gc_u151m2_gdi_blt2, { &graphic_context::gdi_blt2 , true, false } },
            { ws_gc_u151m2_gdi_blt3, { &graphic_context::gdi_blt3 , true, false } },
            { ws_gc_u151m2_gdi_ws_blt2, { &graphic_context::gdi_ws_blt2 , true, false } },
            { ws_gc_u151m2_gdi_ws_blt3, { &graphic_context::gdi_ws_blt3 , true, false } },
            { ws_gc_u151m2_gdi_blt_masked, { &graphic_context::gdi_blt_masked , true, false } },
            { ws_gc_u151m2_gdi_ws_blt_masked, { &graphic_context::gdi_ws_blt_masked , true, false } },
            { ws_gc_u151m2_free, { &graphic_context::free , true, true } }
        };

        static const ws_graphics_context_table_op curr_opcode_handlers = {
            { ws_gc_curr_active, { &graphic_context::active , false, false } },
            { ws_gc_curr_set_clipping_rect, { &graphic_context::set_clipping_rect , false, false } },
            { ws_gc_curr_set_brush_color, { &graphic_context::set_brush_color , false, false } },
            { ws_gc_curr_set_brush_style, { &graphic_context::set_brush_style , false, false } },
            { ws_gc_curr_set_pen_color, { &graphic_context::set_pen_color , false, false } },
            { ws_gc_curr_set_pen_style, { &graphic_context::set_pen_style , false, false } },
            { ws_gc_curr_set_pen_size, { &graphic_context::set_pen_size , false, false } },
            { ws_gc_curr_deactive, { &graphic_context::deactive , false, false } },
            { ws_gc_curr_reset, { &graphic_context::reset , false, false } },
            { ws_gc_curr_use_font, { &graphic_context::use_font, false, false } },
            { ws_gc_curr_discard_font, { &graphic_context::discard_font , false, false } },
            { ws_gc_curr_draw_line, { &graphic_context::draw_line , true, false } },
            { ws_gc_curr_draw_rect, { &graphic_context::draw_rect , true, false } },
            { ws_gc_curr_clear, { &graphic_context::clear , true, false } },
            { ws_gc_curr_clear_rect, { &graphic_context::clear_rect , true, false } },
            { ws_gc_curr_draw_bitmap, { &graphic_context::draw_bitmap , true, false } },
            { ws_gc_curr_draw_bitmap2, { &graphic_context::draw_bitmap_2 , true, false } },
            { ws_gc_curr_draw_bitmap3, { &graphic_context::draw_bitmap_3 , true, false } },
            { ws_gc_curr_ws_draw_bitmap_masked, { &graphic_context::ws_draw_bitmap_masked , true, false } },
            { ws_gc_curr_draw_text, { &graphic_context::draw_text , true, false } },
            { ws_gc_curr_draw_box_text_optimised1, { &graphic_context::draw_box_text_optimised1 , true, false } },
            { ws_gc_curr_draw_box_text_optimised2, { &graphic_context::draw_box_text_optimised2 , true, false } },
            { ws_gc_curr_gdi_blt2, { &graphic_context::gdi_blt2 , true, false } },
            { ws_gc_curr_gdi_blt3, { &graphic_context::gdi_blt3 , true, false } },
            { ws_gc_curr_gdi_ws_blt2, { &graphic_context::gdi_ws_blt2 , true, false } },
            { ws_gc_curr_gdi_ws_blt3, { &graphic_context::gdi_ws_blt3 , true, false } },
            { ws_gc_curr_gdi_blt_masked, { &graphic_context::gdi_blt_masked , true, false } },
            { ws_gc_curr_gdi_ws_blt_masked, { &graphic_context::gdi_ws_blt_masked , true, false } },
            { ws_gc_curr_free, { &graphic_context::free , true, true } }
        };

        epoc::version cli_ver = client->client_version();
        ws_graphics_context_op_handler handler = nullptr;

        bool need_to_set_flushed = false;
        bool need_quit = false;

#define FIND_OPCODE(op, table)                                               \
    auto result = table.find(op);                                            \
    if (result == table.end() || !std::get<0>(result->second)) {             \
        LOG_WARN(SERVICE_WINDOW, "Unimplemented graphics context opcode {}", cmd.header.op); \
        return false;                                                        \
    }                                                                        \
    handler = std::get<0>(result->second);                                   \
    need_to_set_flushed = std::get<1>(result->second);                       \
    need_quit = std::get<2>(result->second);

        kernel_system *kern = client->get_ws().get_kernel_system();

        if (cli_ver.major == 1 && cli_ver.minor == 0) {
            if (cli_ver.build <= WS_OLDARCH_VER) {
                FIND_OPCODE(op, v139u_opcode_handlers)
            } else if (cli_ver.build <= WS_NEWARCH_VER) {
                if (kern->get_epoc_version() <= epocver::epoc81b) {
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
