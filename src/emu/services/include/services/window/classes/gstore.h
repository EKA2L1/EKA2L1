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

#pragma once

#include <common/vecx.h>
#include <common/region.h>

#include <drivers/graphics/common.h>
#include <drivers/itc.h>

#include <cstdint>
#include <memory>

namespace eka2l1::drivers {
    class graphics_driver;
}

namespace eka2l1::epoc {
    class bitmap_cache;
    struct window;

    enum gdi_store_command_opcode : std::uint32_t {
        gdi_store_command_draw_rect,
        gdi_store_command_draw_line,
        gdi_store_command_draw_polygon,
        gdi_store_command_draw_bitmap,
        gdi_store_command_draw_raw_texture,
        gdi_store_command_draw_text,
        gdi_store_command_set_clip_rect_single,
        gdi_store_command_set_clip_rect_multiple,
        gdi_store_command_disable_clip
    };

    struct gdi_store_command_draw_rect_data {
        eka2l1::vec4 color_;
        eka2l1::rect rect_;
    };

    struct gdi_store_command_draw_line_data {
        eka2l1::vec4 color_;
        drivers::pen_style style_;
        eka2l1::vec2 start_;
        eka2l1::vec2 end_;
    };

    struct gdi_store_command_draw_polygon_data {
        eka2l1::vec4 color_;
        drivers::pen_style style_;
        eka2l1::point *points_;
        std::uint32_t point_count_;
    };

    struct gdi_store_command_draw_text_data {
        eka2l1::vec4 color_;
        const char16_t *string_;
        eka2l1::rect text_box_;
        std::uint32_t alignment_;
        void *fbs_font_ptr_;
    };
    
    struct gdi_store_command_draw_raw_texture_data {
        drivers::handle texture_;
        eka2l1::rect dest_rect_;
        std::uint32_t flags_;
    };

    enum gdi_store_command_draw_bitmap_flags {
        GDI_STORE_COMMAND_INVERT_MASK = 1 << 0,
        GDI_STORE_COMMAND_MAIN_RAW = 1 << 1,
        GDI_STORE_COMMAND_MASK_RAW = 1 << 2,
        GDI_STORE_COMMAND_BLIT = 1 << 3
    };

    struct gdi_store_command_draw_bitmap_data {
        void *main_fbs_bitmap_;
        void *mask_fbs_bitmap_;

        eka2l1::rect dest_rect_;
        eka2l1::rect source_rect_;

        drivers::handle main_drv_ = 0;
        drivers::handle mask_drv_ = 0;

        std::uint32_t gdi_flags_;
    };

    struct gdi_store_command_set_clip_rect_single_data {
        eka2l1::rect clipping_rect_;
    };

    struct gdi_store_command_set_clip_rect_multiple_data {
        eka2l1::rect *rects_;
        std::uint32_t rect_count_;
    };

    static constexpr std::size_t MAX_COMMAND_STORE_DATA_SIZE = sizeof(gdi_store_command_draw_bitmap_data);

    struct gdi_store_command {
        gdi_store_command_opcode opcode_;
        std::uint8_t data_[MAX_COMMAND_STORE_DATA_SIZE];
        void *dynamic_pointer_ = nullptr;

        template <typename T>
        T &get_data_struct() {
            return *reinterpret_cast<T*>(data_);
        }
        
        template <typename T>
        const T &get_data_struct_const() const {
            return *reinterpret_cast<const T*>(data_);
        }
    };

    enum gdi_store_command_segment_type {
        gdi_store_command_segment_non_redraw,
        gdi_store_command_segment_pending_redraw,
        gdi_store_command_segment_redraw
    };

    struct gdi_store_command_segment {
        gdi_store_command_segment_type type_;
        std::uint64_t creation_date_;

        common::region region_;
        std::vector<gdi_store_command> commands_;

        std::vector<std::uint8_t*> dynamic_pointers_;
        std::vector<void*> font_objects_;
        std::vector<void*> bitmap_objects_;

        ~gdi_store_command_segment();
    };

    class gdi_store_command_collection {
    private:
        std::vector<std::unique_ptr<gdi_store_command_segment>> segments_;
        gdi_store_command_segment *current_segment_;

    public:
        static constexpr std::uint32_t LIMIT_NON_REDRAW_SEGMENTS = 20;
        static constexpr std::int32_t KEEP_NON_REDRAW_SEGMENTS = 12;
        static constexpr std::uint64_t AGE_LIMIT_NONREDRAW_US = 2000000;

        explicit gdi_store_command_collection();

        gdi_store_command_segment *add_new_segment(const eka2l1::rect &draw_rect, const gdi_store_command_segment_type type_);
        void promote_last_segment();
        
        // Returns true if this must cause an invalidation
        bool clean_old_nonredraw_segments();
        void redraw_done();

        gdi_store_command_segment *get_current_segment() const {
            return current_segment_;
        }

        const std::vector<std::unique_ptr<gdi_store_command_segment>> &get_segments() const {
            return segments_;
        }
    };

    class gdi_command_builder {
    private:
        drivers::graphics_driver *driver_;
        drivers::graphics_command_builder &builder_;
        bitmap_cache &bcache_;
        float scale_factor_;
        eka2l1::vec2 position_;
        common::region clip_;

    public:
        explicit gdi_command_builder(drivers::graphics_driver *drv, drivers::graphics_command_builder &builder, bitmap_cache &bcache,
            const eka2l1::vec2 &position, float scale_factor, const common::region &clip);

        void set_position(const eka2l1::vec2 &pos) {
            position_ = pos;
        }

        void set_clip_region(const common::region &region) {
            clip_ = region;
        }

        void build_segment(const gdi_store_command_segment &segment);
        void build_single_command(const gdi_store_command &command);
        void build_command_draw_rect(const gdi_store_command_draw_rect_data &cmd);
        void build_command_draw_line(const gdi_store_command_draw_line_data &cmd);
        void build_command_draw_polygon(const gdi_store_command_draw_polygon_data &cmd);
        void build_command_draw_text(const gdi_store_command_draw_text_data &cmd);
        void build_command_draw_raw_texture(const gdi_store_command_draw_raw_texture_data &cmd);
        void build_command_draw_bitmap(const gdi_store_command_draw_bitmap_data &cmd);
        void build_command_set_clip_rect_single(const gdi_store_command_set_clip_rect_single_data &cmd);
        void build_command_set_clip_rect_multiple(const gdi_store_command_set_clip_rect_multiple_data &cmd);
    };
}