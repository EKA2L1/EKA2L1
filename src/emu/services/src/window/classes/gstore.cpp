/*
 * Copyright (c) 2022 EKA2L1 Team
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

#include <services/window/classes/gstore.h>
#include <services/window/util.h>
#include <services/window/bitmap_cache.h>

#include <services/fbs/font.h>
#include <services/fbs/fbs.h>
#include <services/fbs/bitmap.h>

#include <common/time.h>
#include <common/algorithm.h>

namespace eka2l1::epoc {
    // NOTE: Must store objects then free ref with local font atlas.
    gdi_store_command_segment::~gdi_store_command_segment() {
        for (std::size_t i = 0; i < font_objects_.size(); i++) {
            reinterpret_cast<fbsfont*>(font_objects_[i])->deref();
        }
        
        for (std::size_t i = 0; i < bitmap_objects_.size(); i++) {
            reinterpret_cast<fbsbitmap*>(bitmap_objects_[i])->deref();
        }
    }

    void gdi_store_command_segment::add_command(gdi_store_command &command) {
        if (command.opcode_ == gdi_store_command_draw_text) {
            auto &data = command.get_data_struct_const<gdi_store_command_draw_text_data>();
            if (data.fbs_font_ptr_) {
                auto ite = std::find(font_objects_.begin(), font_objects_.end(), data.fbs_font_ptr_);

                if (ite == font_objects_.end()) {
                    font_objects_.push_back(data.fbs_font_ptr_);
                    reinterpret_cast<fbsfont*>(data.fbs_font_ptr_)->ref();
                }
            }
        }

        if (command.opcode_ == gdi_store_command_draw_bitmap) {
            auto &data = command.get_data_struct_const<gdi_store_command_draw_bitmap_data>();
            if (data.main_fbs_bitmap_ && ((data.gdi_flags_ & GDI_STORE_COMMAND_MAIN_RAW) == 0)) {
                auto ite = std::find(bitmap_objects_.begin(), bitmap_objects_.end(), data.main_fbs_bitmap_);

                if (ite == bitmap_objects_.end()) {
                    bitmap_objects_.push_back(data.main_fbs_bitmap_);
                    reinterpret_cast<fbsbitmap*>(data.main_fbs_bitmap_)->ref();
                }
            }
            
            if (data.mask_fbs_bitmap_ && ((data.gdi_flags_ & GDI_STORE_COMMAND_MASK_RAW) == 0)) {
                auto ite = std::find(bitmap_objects_.begin(), bitmap_objects_.end(), data.mask_fbs_bitmap_);

                if (ite == bitmap_objects_.end()) {
                    bitmap_objects_.push_back(data.mask_fbs_bitmap_);
                    reinterpret_cast<fbsbitmap*>(data.mask_fbs_bitmap_)->ref();
                }
            }
        }

        commands_.push_back(command);
    }

    gdi_store_command_collection::gdi_store_command_collection()
        : current_segment_(nullptr) {
    }

    gdi_store_command_segment *gdi_store_command_collection::add_new_segment(const eka2l1::rect &draw_rect, const gdi_store_command_segment_type type) {
        std::unique_ptr<gdi_store_command_segment> new_segment = std::make_unique<gdi_store_command_segment>();

        new_segment->type_ = type;
        new_segment->creation_date_ = common::get_current_utc_time_in_microseconds_since_epoch();
        new_segment->region_.add_rect(draw_rect);

        gdi_store_command_segment *new_segment_ptr = new_segment.get();
        segments_.push_back(std::move(new_segment));

        current_segment_ = new_segment_ptr;
        return new_segment_ptr;
    }

    void gdi_store_command_collection::promote_last_segment() {
        if (segments_.empty()) {
            return;
        }

        gdi_store_command_segment *lastest_segment = segments_.back().get();
        if (lastest_segment->type_ != gdi_store_command_segment_pending_redraw) {
            return;
        }

        for (std::size_t i = 0; i < lastest_segment->region_.rects_.size(); i++) {
            for (std::size_t j = 0; j < segments_.size(); ) {
                if (segments_[j]->type_ != gdi_store_command_segment_pending_redraw) {
                    segments_[j]->region_.eliminate(lastest_segment->region_.rects_[i]);

                    if (segments_[j]->region_.empty()) {
                        segments_.erase(segments_.begin() + j);
                    } else {
                        j++;
                    }
                } else {
                    j++;
                }
            }
        }

        lastest_segment->type_ = gdi_store_command_segment_redraw;
    }

    bool gdi_store_command_collection::clean_old_nonredraw_segments() {
        if (segments_.empty()) {
            return false;
        }

        std::uint32_t non_redraw_segment_count = 0;
        for (std::size_t i = 0; i < segments_.size(); i++) {
            if (segments_[i]->type_ == gdi_store_command_segment_non_redraw) {
                non_redraw_segment_count++;
            }
        }

        // If we managed to clean out some old non-redraw segments, we gotta invalidate all the window
        // Because some non-redraw segment may need to stay there.
        bool need_invalidate = false;
        std::int32_t left_to_keep = KEEP_NON_REDRAW_SEGMENTS;

        if (non_redraw_segment_count > LIMIT_NON_REDRAW_SEGMENTS) {
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(segments_.size()); ) {
                if ((segments_[i].get() != current_segment_) && (segments_[i]->type_ == gdi_store_command_segment_non_redraw)) {
                    if (left_to_keep-- > 0) {
                        i++;
                        continue;
                    } else {
                        segments_.erase(segments_.begin() + i);
                        need_invalidate = true;
                    }
                } else {
                    i++;
                }
            }
        }

        if (current_segment_ && (current_segment_->type_ == gdi_store_command_segment_non_redraw)) {
            std::uint64_t current_time = common::get_current_utc_time_in_microseconds_since_epoch();

            // Try to make it able to be cleaned (this routine is actually from OSS)
            if (current_time - current_segment_->creation_date_ > AGE_LIMIT_NONREDRAW_US) {
                // Try to find older segments
                for (std::size_t i = 0; i < segments_.size(); ) {
                    if ((segments_[i].get() != current_segment_) && (segments_[i]->type_ == gdi_store_command_segment_non_redraw)) {
                        if ((current_time - segments_[i]->creation_date_) > AGE_LIMIT_NONREDRAW_US * 2) {
                            segments_.erase(segments_.begin() + i);
                        } else {
                            i++;
                        }
                    } else {
                        i++;
                    }
                }

                current_segment_->creation_date_ = current_time;
                current_segment_ = nullptr;

                need_invalidate = true;
            }
        }

        return need_invalidate;
    }

    void gdi_store_command_collection::redraw_done() {
        current_segment_ = nullptr;
    }

    gdi_command_builder::gdi_command_builder(drivers::graphics_driver *drv, drivers::graphics_command_builder &builder, bitmap_cache &bcache,
        drivers::filter_option texture_filter, const eka2l1::vec2 &position, float scale_factor, const common::region &clip)
        : driver_(drv)
        , builder_(builder)
        , bcache_(bcache)
        , scale_factor_(scale_factor)
        , position_(position)
        , clip_(clip)
        , texture_filter_(texture_filter) {
    }

    void gdi_command_builder::build_segment(const gdi_store_command_segment &segment) {
        for (std::size_t i = 0; i < segment.commands_.size(); i++) {
            build_single_command(segment.commands_[i]);
        }
    }

    void gdi_command_builder::build_single_command(const gdi_store_command &command) {
        switch (command.opcode_) {
        case gdi_store_command_draw_rect:
            build_command_draw_rect(command.get_data_struct_const<gdi_store_command_draw_rect_data>());
            break;

        case gdi_store_command_draw_line:
            build_command_draw_line(command.get_data_struct_const<gdi_store_command_draw_line_data>());
            break;

        case gdi_store_command_draw_polygon:
            build_command_draw_polygon(command.get_data_struct_const<gdi_store_command_draw_polygon_data>());
            break;

        case gdi_store_command_draw_text:
            build_command_draw_text(command.get_data_struct_const<gdi_store_command_draw_text_data>());
            break;

        case gdi_store_command_draw_bitmap:
            build_command_draw_bitmap(command.get_data_struct_const<gdi_store_command_draw_bitmap_data>());
            break;

        case gdi_store_command_disable_clip:
            build_command_disable_clip();
            break;

        case gdi_store_command_set_clip_rect_single:
            build_command_set_clip_rect_single(command.get_data_struct_const<gdi_store_command_set_clip_rect_single_data>());
            break;

        case gdi_store_command_set_clip_rect_multiple:
            build_command_set_clip_rect_multiple(command.get_data_struct_const<gdi_store_command_set_clip_rect_multiple_data>());
            break;

        case gdi_store_command_update_texture:
            build_command_update_texture(command.get_data_struct_const<gdi_store_command_update_texture_data>());
            break;

        default:
            break;
        }
    }

    void gdi_command_builder::build_command_draw_rect(const gdi_store_command_draw_rect_data &cmd) {
        eka2l1::rect scaled_rect = cmd.rect_;
        scaled_rect.top += position_;

        scale_rectangle(scaled_rect, scale_factor_);

        builder_.set_brush_color_detail(cmd.color_);
        builder_.draw_rectangle(scaled_rect);
    }

    void gdi_command_builder::build_command_draw_line(const gdi_store_command_draw_line_data &cmd) {
        eka2l1::point scaled_start = (cmd.start_ + position_) * scale_factor_;
        eka2l1::point scaled_end = (cmd.end_ + position_) * scale_factor_;

        builder_.set_brush_color_detail(cmd.color_);

        // Trying to emulate brush size here. There's more complexity in adding a real variant.
        if (cmd.style_ == drivers::pen_style_solid) {
            if (((scale_factor_ != 1.0f) || ((cmd.pen_size_.x != 1) || (cmd.pen_size_.y != 1))) && ((cmd.start_.x == cmd.end_.x)
                || (cmd.start_.y == cmd.end_.y))) {
                eka2l1::rect draw_rect;
                draw_rect.top = scaled_start - (cmd.pen_size_ * (scale_factor_ / 2.0f));
                if (cmd.start_.x == cmd.end_.x) {
                    if (cmd.start_.y == cmd.end_.y) {
                        draw_rect.size = cmd.pen_size_ * scale_factor_;
                    } else {
                        draw_rect.size = eka2l1::vec2(static_cast<int>(std::roundf(cmd.pen_size_.x * scale_factor_)), scaled_end.y - scaled_start.y + static_cast<int>(std::roundf(cmd.pen_size_.y * scale_factor_ / 2.0f)));
                    }
                } else {
                    draw_rect.size = eka2l1::vec2(scaled_end.x - scaled_start.x + static_cast<int>(std::roundf(cmd.pen_size_.x * scale_factor_ / 2.0f)), static_cast<int>(std::roundf(cmd.pen_size_.y * scale_factor_)));
                }

                builder_.draw_rectangle(draw_rect);
                return;
            }
        }

        builder_.set_pen_style(cmd.style_);
        builder_.draw_line(scaled_start, scaled_end);
    }

    void gdi_command_builder::build_command_draw_polygon(const gdi_store_command_draw_polygon_data &cmd) {
        std::vector<eka2l1::point> copied_points(cmd.point_count_);
        for (std::size_t i = 0; i < cmd.point_count_; i++) {
            copied_points[i] = (cmd.points_[i] + position_) * scale_factor_;
        }

        builder_.set_brush_color_detail(cmd.color_);
        builder_.set_pen_style(cmd.style_);
        builder_.draw_polygons(copied_points.data(), cmd.point_count_);
    }

    void gdi_command_builder::build_command_draw_text(const gdi_store_command_draw_text_data &cmd) {
        builder_.set_brush_color_detail(cmd.color_);
        fbsfont *text_font = reinterpret_cast<fbsfont*>(cmd.fbs_font_ptr_);
        
        std::int16_t scaled_font_size = text_font->of_info.metrics.max_height;
        float scale_to_pass = 1.0f;

        if (text_font->of_info.adapter->vectorizable()) {
            scaled_font_size = static_cast<std::int16_t>(scaled_font_size * scale_factor_);
            
            if ((text_font->atlas.atlas_handle_ != 0) && (scaled_font_size != text_font->atlas.get_char_size())) {
                text_font->atlas.destroy(driver_);
            }
        } else {
            scale_to_pass = scale_factor_;
        }

        if (text_font->atlas.atlas_handle_ == 0) {
            text_font->atlas.init(text_font->of_info.adapter, text_font->of_info.idx, 0x20, 0xFF - 0x20,
                scaled_font_size);
        }

        eka2l1::rect scaled_text_box = cmd.text_box_;
        scaled_text_box.top += position_;

        scale_rectangle(scaled_text_box, scale_factor_);

        text_font->atlas.draw_text(cmd.string_, scaled_text_box, static_cast<epoc::text_alignment>(cmd.alignment_),
            driver_, builder_, scale_to_pass);
    }

    void gdi_command_builder::build_command_draw_raw_texture(const gdi_store_command_draw_raw_texture_data &cmd) {
        eka2l1::rect scaled_dest_rect = cmd.dest_rect_;
        scaled_dest_rect.top += position_;

        scale_rectangle(scaled_dest_rect, scale_factor_);

        builder_.draw_bitmap(cmd.texture_, 0, scaled_dest_rect, eka2l1::rect{}, eka2l1::vec2(0, 0), 0.0f,
            cmd.flags_);
    }

    void gdi_command_builder::build_command_draw_bitmap(const gdi_store_command_draw_bitmap_data &cmd) {
        epoc::bitwise_bitmap *source_bitmap_bw = reinterpret_cast<epoc::bitwise_bitmap*>(cmd.main_fbs_bitmap_);

        if ((cmd.gdi_flags_ & GDI_STORE_COMMAND_MAIN_RAW) == 0) {
            source_bitmap_bw = reinterpret_cast<fbsbitmap*>(cmd.main_fbs_bitmap_)->final_clean()->bitmap_;
        }

        epoc::bitwise_bitmap *mask_bitmap_bw = reinterpret_cast<epoc::bitwise_bitmap*>(cmd.mask_fbs_bitmap_);

        if (mask_bitmap_bw) {
            if ((cmd.gdi_flags_ & GDI_STORE_COMMAND_MASK_RAW) == 0) {
                mask_bitmap_bw = reinterpret_cast<fbsbitmap*>(cmd.mask_fbs_bitmap_)->final_clean()->bitmap_;
            }
        }

        drivers::handle source_bitmap_drv = cmd.main_drv_;
        if (!source_bitmap_drv) {
            source_bitmap_drv = bcache_.add_or_get(driver_, source_bitmap_bw, &builder_);
        }

        drivers::handle mask_bitmap_drv = cmd.mask_drv_;

        if (!mask_bitmap_drv && mask_bitmap_bw) {
            mask_bitmap_drv = bcache_.add_or_get(driver_, mask_bitmap_bw, &builder_);
        }

        eka2l1::rect scaled_dest_rect = cmd.dest_rect_;
        scaled_dest_rect.top += position_;

        eka2l1::rect adjusted_source_rect = cmd.source_rect_;

        if (scaled_dest_rect.size.x == 0) {
            if (adjusted_source_rect.size.x == 0) {
                scaled_dest_rect.size.x = source_bitmap_bw->header_.size_pixels.x;
            } else {
                scaled_dest_rect.size.x = adjusted_source_rect.size.x;
            }
        }

        if (scaled_dest_rect.size.y == 0) {
            if (adjusted_source_rect.size.y == 0) {
                scaled_dest_rect.size.y = source_bitmap_bw->header_.size_pixels.y;
            } else {
                scaled_dest_rect.size.y = adjusted_source_rect.size.y;
            }
        }
        
        scale_rectangle(scaled_dest_rect, scale_factor_);

        // Handle this variant: BitBlt(const TPoint &aDestination, const CFbsBitmap *aBitmap, const TRect &aSource);
        if ((cmd.gdi_flags_ & GDI_STORE_COMMAND_BLIT) &&
            ((cmd.source_rect_.size.y > source_bitmap_bw->header_.size_pixels.y) ||
            (cmd.source_rect_.size.x > source_bitmap_bw->header_.size_pixels.x))) {
            if (!mask_bitmap_drv) {
                builder_.set_brush_color_detail(eka2l1::vec4(255, 255, 255, 255));
                builder_.draw_rectangle(scaled_dest_rect);
            }

            if (cmd.source_rect_.size.y > source_bitmap_bw->header_.size_pixels.y) {
                adjusted_source_rect.size.y = source_bitmap_bw->header_.size_pixels.y;
            }
            
            if (cmd.source_rect_.size.x > source_bitmap_bw->header_.size_pixels.x) {
                adjusted_source_rect.size.x = source_bitmap_bw->header_.size_pixels.x;
            }

            scaled_dest_rect.size = adjusted_source_rect.size * scale_factor_;
        }

        bool swizzle_alteration = false;

        const bool alpha_blending = mask_bitmap_bw && ((mask_bitmap_bw->settings_.current_display_mode() == epoc::display_mode::gray256)
            || (epoc::is_display_mode_alpha(mask_bitmap_bw->settings_.current_display_mode())));

        std::uint32_t flags = 0;

        if ((cmd.gdi_flags_ & GDI_STORE_COMMAND_INVERT_MASK) && !alpha_blending) {
            flags |= drivers::bitmap_draw_flag_invert_mask;
        }

        if (!alpha_blending) {
            flags |= drivers::bitmap_draw_flag_flat_blending;
        }

        if (mask_bitmap_bw) {
            builder_.set_feature(drivers::graphics_feature::blend, true);

            // For non alpha blending we always want to take color buffer's alpha.
            builder_.blend_formula(drivers::blend_equation::add, drivers::blend_equation::add,
                drivers::blend_factor::frag_out_alpha, drivers::blend_factor::one_minus_frag_out_alpha,
                (alpha_blending ? drivers::blend_factor::one : drivers::blend_factor::one),
                (alpha_blending ? drivers::blend_factor::one_minus_frag_out_alpha : drivers::blend_factor::one));
        }

        if (mask_bitmap_bw && !alpha_blending && !epoc::is_display_mode_alpha(mask_bitmap_bw->settings_.current_display_mode())) {
            swizzle_alteration = true;
            builder_.set_swizzle(mask_bitmap_drv, drivers::channel_swizzle::red, drivers::channel_swizzle::green,
                drivers::channel_swizzle::blue, drivers::channel_swizzle::red);
        }

        builder_.set_texture_filter(source_bitmap_drv, false, texture_filter_);
        builder_.set_texture_filter(source_bitmap_drv, true, texture_filter_);

        if (mask_bitmap_drv) {
            builder_.set_texture_filter(mask_bitmap_drv, false, texture_filter_);
            builder_.set_texture_filter(mask_bitmap_drv, true, texture_filter_);
        }

        builder_.draw_bitmap(source_bitmap_drv, mask_bitmap_drv, scaled_dest_rect, adjusted_source_rect,
            eka2l1::vec2(0, 0), 0.0f, flags);

        if (mask_bitmap_bw) {
            builder_.set_feature(drivers::graphics_feature::blend, false);
        }
        
        if (swizzle_alteration) {
            builder_.set_swizzle(mask_bitmap_drv, drivers::channel_swizzle::red, drivers::channel_swizzle::green,
                drivers::channel_swizzle::blue, drivers::channel_swizzle::alpha);
        }
    }

    void gdi_command_builder::build_command_set_clip_rect_single(const gdi_store_command_set_clip_rect_single_data &cmd) {
        eka2l1::rect rect_advanced = cmd.clipping_rect_;
        rect_advanced.top += position_;

        common::region clipped;
        clipped.add_rect(rect_advanced);
        clipped = clipped.intersect(clip_);

        builder_.clip_bitmap_region(clipped, scale_factor_);
    }

    void gdi_command_builder::build_command_set_clip_rect_multiple(const gdi_store_command_set_clip_rect_multiple_data &cmd) {
        common::region clipped;
        clipped.rects_.insert(clipped.rects_.begin(), cmd.rects_, cmd.rects_ + cmd.rect_count_);
        clipped.advance(position_);
        clipped = clipped.intersect(clip_);

        builder_.clip_bitmap_region(clipped, scale_factor_);
    }

    void gdi_command_builder::build_command_disable_clip() {
        if (clip_.empty()) {
            builder_.set_feature(drivers::graphics_feature::stencil_test, false);
            builder_.set_feature(drivers::graphics_feature::clipping, false);
        } else {
            builder_.clip_bitmap_region(clip_, scale_factor_);
        }
    }

    void gdi_command_builder::build_command_update_texture(const gdi_store_command_update_texture_data &cmd) {
        if (cmd.destroy_handle_) {
            builder_.destroy_bitmap(cmd.destroy_handle_);
        }

        builder_.update_bitmap(cmd.handle_, reinterpret_cast<const char*>(cmd.texture_data_), cmd.texture_size_,
            eka2l1::vec2(0, 0), cmd.dim_, cmd.pixel_per_line_, false);

        if (cmd.do_swizz_) {
            builder_.set_swizzle(cmd.handle_, cmd.swizz_[0], cmd.swizz_[1], cmd.swizz_[2], cmd.swizz_[3]);
        }
    }
}