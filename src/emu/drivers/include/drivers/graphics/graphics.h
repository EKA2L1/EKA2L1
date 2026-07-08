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
#include <drivers/graphics/arena.h>
#include <drivers/graphics/common.h>
#include <drivers/itc.h>

#include <functional>
#include <memory>
#include <vector>

namespace eka2l1::drivers {
    enum graphics_driver_opcode : std::uint16_t {
        // Mode -1: Miscs
        graphics_driver_clip_rect,
        graphics_driver_clip_bitmap_rect,
        graphics_driver_set_feature,
        graphics_driver_set_viewport,
        graphics_driver_set_bitmap_viewport,
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
        // Don't mix it with advance mode
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
        graphics_driver_clip_region,

        // Mode 1: Advance - Lower access to functions
        graphics_driver_create_shader_module,
        graphics_driver_create_shader_program,
        graphics_driver_create_renderbuffer,
        graphcis_driver_create_framebuffer,
        graphics_driver_create_texture,
        graphics_driver_create_buffer,
        graphics_driver_destroy_object,
        graphics_driver_set_texture_filter,
        graphics_driver_set_texture_wrap,
        graphics_driver_generate_mips,
        graphics_driver_set_max_mip_level,
        graphics_driver_set_texture_anisotrophy,
        graphics_driver_use_program,
        graphics_driver_set_uniform,
        graphics_driver_bind_texture,
        graphics_driver_bind_vertex_buffers,
        graphics_driver_bind_index_buffer,
        graphics_driver_bind_framebuffer,
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
        graphics_driver_set_framebuffer_color_buffer,
        graphics_driver_set_framebuffer_depth_stencil_buffer,
        graphics_driver_set_blend_colour,
        graphics_driver_read_framebuffer,
        graphics_driver_backup_state, // Backup all possible state to a struct
        graphics_driver_restore_state, // Restore previously backup data
        graphics_driver_execute_command_list // Execute an embedded sub-command-list
    };

    enum graphics_driver_extension {
        graphics_driver_extension_anisotrophy_filtering = 1 << 0,
        graphics_driver_extension_float_precision_qualifier = 1 << 1
    };

    enum graphics_driver_extension_query {
        graphics_driver_extension_query_max_texture_max_anisotrophy
    };

    using display_hook = std::function<void()>;

    class graphics_driver : public driver {
        graphic_api api_;

    protected:
        display_hook disp_hook_;

        /// Three-tier arena pool system:
        ///   - small:  64 KB → trivial single commands (resize, set_filter)
        ///   - medium: 2 MB  → screen updates, batch cleanup, moderate data
        ///   - large:  8 MB → tree walks, large texture uploads
        arena_pool<4> large_arena_pool_{8 * 1024 * 1024};
        arena_pool<8> medium_arena_pool_{2 * 1024 * 1024};
        arena_pool<16> small_arena_pool_{64 * 1024};

        /// Deferred destroy queue. Trivial single-destroy operations are
        /// batched here to avoid wasting an entire arena on one command.
        /// Flushed automatically by acquire_builder().
        std::vector<drivers::handle> deferred_destroys_;

        /** \brief Submit all queued deferred destroys as one batch. */
        void flush_deferred_destroys() {
            if (deferred_destroys_.empty()) {
                return;
            }

            auto builder = acquire_builder_raw(small_arena_pool_);
            for (const auto h : deferred_destroys_) {
                builder.destroy(h);
            }
            deferred_destroys_.clear();

            command_list list = builder.retrieve_command_list();
            submit_command_list(list);
        }

        /** \brief Acquire an arena-backed builder from a specific pool, without flushing deferred work. */
        template <std::size_t N>
        graphics_command_builder acquire_builder_raw(arena_pool<N> &pool) {
            graphics_command_builder builder;
            arena *a = pool.acquire();
            if (a) {
                builder.set_building_arena(a);
            }
            // If a is nullptr, the builder stays in heap mode — safe fallback
            // that avoids deadlock when the pool is temporarily exhausted.
            return builder;
        }

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
        virtual void update_surface_size(const eka2l1::vec2 &size) = 0;

        /**
         * \brief Submit a command list.
         * 
         * The list object will be copied within the function, and can be safely delete after.
         *
         * \param cmd_list     Command list to submit.
         */
        virtual void submit_command_list(command_list &cmd_list) = 0;

        /**
         * \brief Create a large-arena command builder (16 MB arena).
         *
         * For tree-walk redraws, large texture uploads, and heavy operations.
         * Automatically flushes pending deferred destroys first.
         * For lighter operations, prefer acquire_builder_medium() or
         * acquire_builder_small(). For single destroys, prefer defer_destroy().
         */
        graphics_command_builder acquire_builder() {
            flush_deferred_destroys();
            return acquire_builder_raw(large_arena_pool_);
        }

        /**
         * \brief Create a medium-arena command builder (4 MB arena).
         *
         * For screen buffer updates, batch shader/resource cleanup, and
         * moderate drawing (sync_from_bitmap, scroll, etc.).
         */
        graphics_command_builder acquire_builder_medium() {
            return acquire_builder_raw(medium_arena_pool_);
        }

        /**
         * \brief Create a small-arena command builder (64 KB arena).
         *
         * For trivial single-command operations (set_texture_filter,
         * resize_bitmap, etc.). Not suitable for any data uploads.
         */
        graphics_command_builder acquire_builder_small() {
            return acquire_builder_raw(small_arena_pool_);
        }

        /**
         * \brief Queue a handle for deferred destruction.
         *
         * Instead of acquiring an entire arena-backed builder just to call
         * destroy(), use this to batch multiple destroys together. The
         * actual destruction happens when the next acquire_builder() is
         * called, or explicitly via flush_deferred_destroys().
         *
         * This avoids wasting a 16 MB arena on a single 96-byte command.
         */
        void defer_destroy(const drivers::handle h) {
            deferred_destroys_.push_back(h);
        }

        /**
         * \brief Called by the render thread after consuming an arena-backed
         *        command list. Returns the arena to the correct pool.
         */
        void signal_arena_consumed(arena *a) {
            small_arena_pool_.release(a);
            medium_arena_pool_.release(a);
            large_arena_pool_.release(a);
        }

        virtual void set_upscale_shader(const std::string &name) = 0;
        virtual std::string get_active_upscale_shader() const = 0;

        virtual bool support_extension(const graphics_driver_extension ext) = 0;
        virtual bool query_extension_value(const graphics_driver_extension_query query, void *data_ptr) = 0;
    };

    using graphics_driver_ptr = std::unique_ptr<graphics_driver>;

    graphics_driver_ptr create_graphics_driver(const graphic_api api, const window_system_info &info);
};
