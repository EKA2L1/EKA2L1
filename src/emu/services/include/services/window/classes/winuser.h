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

#pragma once

#include <services/window/classes/winbase.h>
#include <services/window/common.h>

#include <common/linked.h>
#include <common/region.h>

#include <optional>

namespace eka2l1 {
    struct fbsbitmap;

    namespace drivers {
        class graphics_command_list_builder;
    }
}

namespace eka2l1::epoc {
    struct graphic_context;
    struct window_group;
    struct dsa;

    struct canvas_interface {
        virtual std::uint32_t redraw_priority(int *shift = nullptr) = 0;
        virtual eka2l1::vec2 absolute_position() const = 0;
        virtual eka2l1::vec2 get_origin() = 0;
    };

    struct canvas_base : public epoc::window, public canvas_interface {
        epoc::display_mode dmode;
        epoc::window_type win_type;

        // The position
        eka2l1::vec2 pos{ 0, 0 };
        eka2l1::rect abs_rect;

        bool resize_needed;
        bool clear_color_enable;

        common::roundabout attached_contexts;

        std::uint32_t clear_color;
        std::uint32_t filter;

        eka2l1::vec2 cursor_pos;

        std::uint64_t driver_win_id;
        common::region visible_region;

        int shadow_height;

        std::uint32_t max_pointer_buffer_;
        std::vector<epoc::event> pointer_buffer_;

        std::uint64_t last_draw_;
        std::uint64_t last_fps_sync_;
        std::uint64_t fps_count_;

        // NOTE: If you ever want to access this and call a function that can directly affect this list elements, copy it first
        std::vector<dsa*> directs_;

        explicit canvas_base(window_server_client_ptr client, screen *scr, window *parent, const epoc::window_type type_of_window, const epoc::display_mode dmode, const std::uint32_t client_handle);
        ~canvas_base() override;

        virtual void wipeout();
        virtual bool draw(drivers::graphics_command_list_builder *builder);

        virtual void on_activate() = 0;
        virtual void handle_extent_changed(const bool size_changed, const bool pos_changed) = 0;

        void prepare_driver_bitmap();

        epoc::display_mode display_mode() const;
        eka2l1::vec2 absolute_position() const override;
        eka2l1::vec2 get_origin() override;

        std::uint32_t redraw_priority(int *shift = nullptr) override;
        eka2l1::rect bounding_rect() const;
        eka2l1::rect absolute_rect() const;
        eka2l1::vec2 size() const;

        /**
         * \brief Set window extent in screen space.
         * 
         * \param top   The position of the window on the screen coords, in pixels.
         * \param size  The size of the window, in pixel.
         */
        void set_extent(const eka2l1::vec2 &top, const eka2l1::vec2 &size);
        void recalculate_absolute_position(const eka2l1::vec2 &diff);

        bool is_visible() const;
        bool can_be_physically_seen() const;

        bool is_faded() const {
            return (flags & flags_faded);
        }

        bool content_changed() const {
            return (flags & flag_content_changed);
        }

        void content_changed(const bool changed) {
            if (changed) {
                flags |= flag_content_changed;
            } else {
                flags &= ~flag_content_changed;
            }
        }

        bool is_dsa_active() const;

        void add_dsa_active(dsa *dsa);
        void remove_dsa_active(dsa *dsa);

        /**
         * @brief Set window visibility.
         * 
         * This will trigger a screen redraw if the visibility is changed.
         */
        void set_visible(const bool vis);

        /**
         * @brief Action that this window does when its content is modified.
         */
        virtual void take_action_on_change(kernel::thread *drawer);

        void queue_event(const epoc::event &evt) override;

        void set_non_fading(service::ipc_context &context, ws_cmd &cmd);
        void set_size(service::ipc_context &context, ws_cmd &cmd);
        void set_transparency_alpha_channel(service::ipc_context &context, ws_cmd &cmd);
        void activate(service::ipc_context &context, ws_cmd &cmd);
        void free(service::ipc_context &context, ws_cmd &cmd);
        void alloc_pointer_buffer(service::ipc_context &context, ws_cmd &cmd);

        epoc::window_group *get_group();

        bool execute_command_detail(service::ipc_context &ctx, ws_cmd &cmd, bool &did);
        bool execute_command(service::ipc_context &ctx, ws_cmd &cmd) override;
    };

    struct top_canvas : public epoc::window, public canvas_interface {
        explicit top_canvas(window_server_client_ptr client, screen *scr, window *parent);

        std::uint32_t redraw_priority(int *shift = nullptr) override;
        eka2l1::vec2 get_origin() override;
        eka2l1::vec2 absolute_position() const override;
    };

    struct blank_canvas : public canvas_base {
        explicit blank_canvas(window_server_client_ptr client, screen *scr, window *parent,
            const epoc::display_mode dmode, const std::uint32_t client_handle);

        void on_activate() override {}
        void handle_extent_changed(const bool size_changed, const bool pos_changed) override {}
        
        bool draw(drivers::graphics_command_list_builder *builder) override;
    };

    // Canvas that data is backed using a bitmap
    struct bitmap_backed_canvas: public canvas_base {
        fbsbitmap *bitmap_;

        void create_backed_bitmap();

        void on_activate() override {}
        void handle_extent_changed(const bool size_changed, const bool pos_changed) override;

        explicit bitmap_backed_canvas(window_server_client_ptr client, screen *scr, window *parent,
            const epoc::display_mode dmode, const std::uint32_t client_handle);

        ~bitmap_backed_canvas() override;

        void bitmap_handle(service::ipc_context &context, ws_cmd &cmd);
        void update_screen(service::ipc_context &context, ws_cmd &cmd);
        bool execute_command(service::ipc_context &context, ws_cmd &cmd) override;
        void take_action_on_change(kernel::thread *drawer) override;

        void sync_from_bitmap(std::optional<common::region> region = std::nullopt);
    };

    struct free_modify_canvas : public canvas_base {
        common::region redraw_region;
        eka2l1::rect redraw_rect_curr;

        explicit free_modify_canvas(window_server_client_ptr client, screen *scr, window *parent,
            const epoc::display_mode dmode, const std::uint32_t client_handle);

        void invalidate(const eka2l1::rect &irect);
        void on_activate() override;
        void handle_extent_changed(const bool size_changed, const bool pos_changed) override;

        // ===================== OPCODE IMPLEMENTATIONS ===========================
        void begin_redraw(service::ipc_context &context, ws_cmd &cmd);
        void end_redraw(service::ipc_context &context, ws_cmd &cmd);
        bool clear_redraw_store();
        void store_draw_commands(service::ipc_context &context, ws_cmd &cmd);
        void invalidate(service::ipc_context &context, ws_cmd &cmd);
        void get_invalid_region_count(service::ipc_context &context, ws_cmd &cmd);
        void get_invalid_region(service::ipc_context &context, ws_cmd &cmd);

        bool execute_command(service::ipc_context &context, ws_cmd &cmd) override;
    };
}
