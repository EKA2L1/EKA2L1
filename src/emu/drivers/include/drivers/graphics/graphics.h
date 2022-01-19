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
#include <drivers/itc.h>

#include <functional>
#include <memory>

namespace eka2l1::drivers {
    enum graphics_driver_opcode : std::uint16_t {
        // Mode -1: Miscs
        graphics_driver_clip_rect,
        graphics_driver_set_feature,
        graphics_driver_set_viewport,
        graphics_driver_blend_formula,
        graphics_driver_depth_pass_condition,
        graphics_driver_depth_set_mask,
        graphics_driver_stencil_pass_condition,
        graphics_driver_stencil_set_action,
        graphics_driver_stencil_set_mask,
        graphics_driver_set_front_face_rule,
        graphics_driver_set_swapchain_size,
        graphics_driver_set_ortho_size,
        graphics_driver_cull_face,

        // Mode 0: Immediate - Draw direct 2D elements to screen
        graphics_driver_clear,
        graphics_driver_create_bitmap,
        graphics_driver_destroy_bitmap,
        graphics_driver_bind_bitmap,
        graphics_driver_set_brush_color,
        graphics_driver_update_bitmap,
        graphics_driver_update_texture,
        graphics_driver_draw_bitmap,
        graphics_driver_draw_rectangle,
        graphics_driver_draw_line,
        graphics_driver_draw_polygon,
        graphics_driver_set_point_size,
        graphics_driver_set_pen_style,
        graphics_driver_resize_bitmap,
        graphics_driver_read_bitmap,

        // Mode 1: Advance - Lower access to functions
        graphics_driver_create_shader_module,
        graphics_driver_create_shader_program,
        graphics_driver_create_texture,
        graphics_driver_create_buffer,
        graphics_driver_destroy_object,
        graphics_driver_set_texture_filter,
        graphics_driver_set_texture_wrap,
        graphics_driver_generate_mips,
        graphics_driver_set_max_mip_level,
        graphics_driver_use_program,
        graphics_driver_set_uniform,
        graphics_driver_bind_texture,
        graphics_driver_bind_vertex_buffers,
        graphics_driver_bind_index_buffer,
        graphics_driver_set_texture_for_shader,
        graphics_driver_draw_array,
        graphics_driver_draw_indexed,
        graphics_driver_update_buffer,
        graphics_driver_set_state,
        graphics_driver_display,
        graphics_driver_set_swizzle,
        graphics_driver_set_color_mask,
        graphics_driver_set_depth_func,
        graphics_driver_set_line_width,
        graphics_driver_create_input_descriptor,
        graphics_driver_bind_input_descriptor,
        graphics_driver_set_depth_bias,
        graphics_driver_set_depth_range,
        graphics_driver_backup_state, // Backup all possible state to a struct
        graphics_driver_restore_state // Restore previously backup data
    };

    using display_hook = std::function<void()>;

    class graphics_driver : public driver {
        graphic_api api_;

    protected:
        display_hook disp_hook_;

    public:
        explicit graphics_driver(graphic_api api)
            : api_(api) {}

        virtual ~graphics_driver() {
        }

        const graphic_api get_current_api() const {
            return api_;
        }

        virtual bool is_stricted() const {
            return false;
        }

        /**
         * \brief Set a hook when display function is called.
         *
         * On Vulkan, display may be done using vkQueueDisplayKHR, then you can hook to do things like for example,
         * polling window events.
         *
         * On OpenGL, this hook is expected to swap buffers and also do other things.
         *
         * \param hook    Contains function to hook.
         */
        void set_display_hook(display_hook hook) {
            disp_hook_ = hook;
        }

        virtual void update_bitmap(drivers::handle h, const std::size_t size, const eka2l1::vec2 &offset,
            const eka2l1::vec2 &dim, const void *data, const std::size_t pixels_per_line = 0)
            = 0;

        virtual void set_viewport(const eka2l1::rect &viewport) = 0;
        virtual void update_surface(void *surface) = 0;

        /**
         * \brief Submit a command list.
         * 
         * The list object will be copied within the function, and can be safely delete after.
         *
         * \param cmd_list     Command list to submit.
         */
        virtual void submit_command_list(command_list &cmd_list) = 0;
    };

    using graphics_driver_ptr = std::unique_ptr<graphics_driver>;

    graphics_driver_ptr create_graphics_driver(const graphic_api api, const window_system_info &info);
};
