/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <common/vecx.h>
#include <drivers/driver.h>
#include <drivers/graphics/common.h>

#include <memory>

namespace eka2l1::drivers {
    enum class blend_equation {
        add,
        sub,
        isub
    };

    enum class blend_factor {
        one = 0,
        zero = 1,
        frag_out_alpha = 2,
        one_minus_frag_out_alpha = 3,
        current_alpha = 4,
        one_minus_current_alpha = 5
    };

    enum clear_bits: std::uint8_t {
        clear_bit_color_buffer = 1 << 0,
        clear_bit_depth_buffer = 1 << 1
    };

    enum graphics_driver_opcode : std::uint16_t {
        // Mode -1: Miscs
        graphics_driver_invalidate_rect,
        graphics_driver_set_invalidate,
        graphics_driver_set_viewport,
        graphics_driver_set_blend,
        graphics_driver_set_depth,
        graphics_driver_set_cull,
        graphics_driver_blend_formula,
        graphics_driver_set_back_face_rule,

        // Mode 0: Immediate - Draw direct 2D elements to screen
        graphics_driver_clear,
        graphics_driver_create_bitmap,
        graphics_driver_destroy_bitmap,
        graphics_driver_bind_bitmap,
        graphics_driver_set_brush_color,
        graphics_driver_update_bitmap,
        graphics_driver_draw_bitmap,
        graphics_driver_resize_bitmap,

        // Mode 1: Advance - Lower access to functions
        graphics_driver_create_program,
        graphics_driver_create_texture,
        graphics_driver_create_buffer,
        graphics_driver_use_program,
        graphics_driver_set_uniform,
        graphics_driver_bind_texture,
        graphics_driver_bind_buffer,
        graphics_driver_draw_indexed,
        graphics_driver_update_buffer,
        graphics_driver_set_state,
        graphics_driver_attach_descriptors,
        graphics_driver_backup_state, // Backup all possible state to a struct
        graphics_driver_restore_state // Restore previously backup data
    };

    class graphics_driver : public driver {
        graphic_api api_;

    public:
        explicit graphics_driver(graphic_api api)
            : api_(api) {}

        const graphic_api get_current_api() const {
            return api_;
        }

        virtual void update_bitmap(drivers::handle h, const std::size_t size, const eka2l1::vec2 &offset,
            const eka2l1::vec2 &dim, const int bpp, const void *data)
            = 0;

        virtual void attach_descriptors(drivers::handle h, const int stride, const bool instance_move, const attribute_descriptor *descriptors,
            const int descriptor_count)
            = 0;

        virtual void set_viewport(const eka2l1::rect &viewport) = 0;

        virtual std::unique_ptr<graphics_command_list> new_command_list() = 0;

        virtual std::unique_ptr<graphics_command_list_builder> new_command_builder(graphics_command_list *list) = 0;

        /**
         * \brief Submit a command list.
         * 
         * The list object will be copied within the function, and can be safely delete after.
         *
         * \param command_list     Command list to submit.
         */
        virtual void submit_command_list(graphics_command_list &command_list) = 0;
    };

    using graphics_driver_ptr = std::shared_ptr<graphics_driver>;

    bool init_graphics_library(graphic_api api);

    graphics_driver_ptr create_graphics_driver(const graphic_api api);
};
