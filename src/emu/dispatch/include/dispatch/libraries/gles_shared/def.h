/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include <dispatch/libraries/egl/def.h>
#include <dispatch/libraries/gles_shared/consts.h>
#include <dispatch/def.h>

#include <common/container.h>
#include <common/vecx.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <stack>
#include <tuple>

namespace eka2l1 {
    class system;
}

namespace eka2l1::dispatch {
    #define FIXED_32_TO_FLOAT(x) (float)(x) / 65536.0f

    enum gles_object_type {
        GLES_OBJECT_UNDEFINED,
        GLES_OBJECT_TEXTURE,
        GLES_OBJECT_BUFFER,
        GLES_OBJECT_SHADER,
        GLES_OBJECT_PROGRAM,
        GLES_OBJECT_RENDERBUFFER,
        GLES_OBJECT_FRAMEBUFFER
    };

    struct egl_context_es_shared;

    struct gles_driver_object {
    protected:
        egl_context_es_shared &context_;
        drivers::handle driver_handle_;
        std::uint32_t client_handle_;

    public:
        explicit gles_driver_object(egl_context_es_shared &ctx);
        virtual ~gles_driver_object() = default;

        void assign_handle(const drivers::handle h) {
            driver_handle_ = h;
        }

        void assign_client_handle(const std::uint32_t h) {
            client_handle_ = h;
        }

        const std::uint32_t client_handle() const {
            return client_handle_;
        }

        drivers::handle handle_value() const {
            return driver_handle_;
        }

        virtual gles_object_type object_type() const {
            return GLES_OBJECT_UNDEFINED;
        }
    };

    enum gles_driver_texture_type {
        GLES_DRIVER_TEXTURE_TYPE_NONE = 0,
        GLES_DRIVER_TEXTURE_TYPE_2D = 1,
        GLES_DRIVER_TEXTURE_TYPE_CUBE = 2
    };

    struct gles_driver_texture : public gles_driver_object {
    private:
        std::uint32_t internal_format_;
        std::uint32_t min_filter_;
        std::uint32_t mag_filter_;
        std::uint32_t wrap_s_;
        std::uint32_t wrap_t_;
        std::uint32_t mip_count_;
        float max_anisotrophy_;
        float current_scale_;

        bool auto_regen_mipmap_;
        bool mipmap_gened_;

        eka2l1::vec2 size_;
        gles_driver_texture_type texture_type_;

    public:
        explicit gles_driver_texture(egl_context_es_shared &ctx);
        ~gles_driver_texture() override;

        std::uint32_t try_bind(const std::uint32_t type);
        std::uint32_t target_matched(const std::uint32_t type);

        void set_internal_format(const std::uint32_t format) {
            internal_format_ = format;
        }

        std::uint32_t internal_format() const {
            return internal_format_;
        }

        void set_size(const eka2l1::vec2 size) {
            size_ = size;
        }

        void set_min_filter(const std::uint32_t min_filter) {
            min_filter_ = min_filter;
        }

        std::uint32_t get_min_filter() const {
            return min_filter_;
        }

        void set_mag_filter(const std::uint32_t mag_filter) {
            mag_filter_ = mag_filter;
        }

        std::uint32_t get_mag_filter() const {
            return mag_filter_;
        }

        void set_wrap_s(const std::uint32_t wrap_s) {
            wrap_s_ = wrap_s;
        }

        std::uint32_t get_wrap_s() const {
            return wrap_s_;
        }

        void set_wrap_t(const std::uint32_t wrap_t) {
            wrap_t_ = wrap_t;
        }

        std::uint32_t get_wrap_t() const {
            return wrap_t_;
        }

        void set_mip_count(const std::uint32_t count) {
            mip_count_ = count;
        }

        std::uint32_t get_mip_count() const {
            return mip_count_;
        }

        bool auto_regenerate_mipmap() const {
            return auto_regen_mipmap_;
        }

        void set_auto_regenerate_mipmap(const bool opt) {
            auto_regen_mipmap_ = opt;
        }

        void set_mipmap_generated(const bool opt) {
            mipmap_gened_ = opt;
        }

        const bool is_mipmap_generated() const {
            return mipmap_gened_;
        }

        float max_anisotrophy() const {
            return max_anisotrophy_;
        }

        void set_max_anisotrophy(const float val) {
            max_anisotrophy_ = val;
        }

        eka2l1::vec2 size() const {
            return size_;
        }

        gles_object_type object_type() const override {
            return GLES_OBJECT_TEXTURE;
        }

        gles_driver_texture_type texture_type() const {
            return texture_type_;
        }

        void try_upscale();
    };

    struct gles_driver_buffer : public gles_driver_object {
    private:
        std::uint32_t data_size_;

    public:
        explicit gles_driver_buffer(egl_context_es_shared &ctx);
        ~gles_driver_buffer() override;

        void assign_data_size(const std::uint32_t data_size) {
            data_size_ = data_size;
        }

        const std::uint32_t data_size() const {
            return data_size_;
        }

        gles_object_type object_type() const override {
            return GLES_OBJECT_BUFFER;
        }
    };

    struct gles_vertex_attrib {
        std::int32_t size_;
        std::uint32_t data_type_;
        std::int32_t stride_;
        std::uint32_t offset_;
        std::uint32_t buffer_obj_ = 0;
    };
    
    // General idea from PPSSPP (thanks!)
    struct gles_buffer_pusher {
    public:
        static constexpr std::uint8_t MAX_BUFFER_SLOT = 4;

    private:
        drivers::handle buffers_[MAX_BUFFER_SLOT];
        std::size_t used_size_[MAX_BUFFER_SLOT];
        std::uint8_t *data_[MAX_BUFFER_SLOT];

        std::uint8_t current_buffer_;
        std::size_t size_per_buffer_;

    public:
        explicit gles_buffer_pusher();

        bool is_initialized() const {
            return (size_per_buffer_ != 0);
        }

        void initialize(drivers::graphics_driver *drv, const std::size_t size_per_buffer);
        void destroy(drivers::graphics_command_builder &builder);
        void done_frame();

        drivers::handle push_buffer(const std::uint8_t *data, const std::size_t buffer_size, std::size_t &buffer_offset);
        drivers::handle push_buffer(const std::uint8_t *data, const gles_vertex_attrib &attrib, const std::int32_t first_index, const std::size_t vert_count, std::size_t &buffer_offset);

        void flush(drivers::graphics_command_builder &builder);
    };

    using gles_driver_object_instance = std::unique_ptr<gles_driver_object>;

    enum gles_get_data_type {
        GLES_GET_DATA_TYPE_INTEGER,
        GLES_GET_DATA_TYPE_FLOAT,
        GLES_GET_DATA_TYPE_FIXED,
        GLES_GET_DATA_TYPE_BOOLEAN
    };

    struct egl_context_es_shared : public egl_context {
    protected:
        virtual bool retrieve_vertex_buffer_slot(std::vector<drivers::handle> &vertex_buffers_alloc, drivers::graphics_driver *drv,
            kernel::process *crr_process, const gles_vertex_attrib &attrib, const std::int32_t first_index, const std::int32_t vcount,
            std::uint32_t &res, int &offset, bool &attrib_not_persistent);

    public:
        float clear_color_[4];
        float clear_depth_;
        std::int32_t clear_stencil_;
        
        std::uint32_t active_texture_unit_;
        std::uint32_t binded_array_buffer_handle_;
        std::uint32_t binded_element_array_buffer_handle_;

        drivers::rendering_face active_cull_face_;
        drivers::rendering_face_determine_rule active_front_face_rule_;
        drivers::blend_factor source_blend_factor_rgb_;
        drivers::blend_factor dest_blend_factor_rgb_;
        drivers::blend_factor source_blend_factor_a_;
        drivers::blend_factor dest_blend_factor_a_;
        drivers::blend_equation blend_equation_rgb_;
        drivers::blend_equation blend_equation_a_;

        common::identity_container<gles_driver_object_instance> objects_;

        std::stack<drivers::handle> texture_pools_2d_;
        std::stack<drivers::handle> texture_pools_cube_;
        std::stack<drivers::handle> buffer_pools_;
        
        // Viewport
        eka2l1::rect viewport_bl_;
        eka2l1::rect scissor_bl_;

        // Lines
        float line_width_;

        enum {
            NON_SHADER_STATE_BLEND_ENABLE = 1 << 0,
            NON_SHADER_STATE_COLOR_LOGIC_OP_ENABLE = 1 << 1,
            NON_SHADER_STATE_CULL_FACE_ENABLE = 1 << 2,
            NON_SHADER_STATE_DEPTH_TEST_ENABLE = 1 << 4,
            NON_SHADER_STATE_STENCIL_TEST_ENABLE = 1 << 5,
            NON_SHADER_STATE_LINE_SMOOTH = 1 << 6,
            NON_SHADER_STATE_DITHER = 1 << 7,
            NON_SHADER_STATE_SCISSOR_ENABLE = 1 << 8,
            NON_SHADER_STATE_POLYGON_OFFSET_FILL = 1 << 9,
            NON_SHADER_STATE_MULTISAMPLE = 1 << 10,
            NON_SHADER_STATE_SAMPLE_ALPHA_TO_COVERAGE = 1 << 11,
            NON_SHADER_STATE_SAMPLE_ALPHA_TO_ONE = 1 << 12,
            NON_SHADER_STATE_SAMPLE_COVERAGE = 1 << 13,

            STATE_CHANGED_CULL_FACE = 1 << 0,
            STATE_CHANGED_SCISSOR_RECT = 1 << 1,
            STATE_CHANGED_VIEWPORT_RECT = 1 << 2,
            STATE_CHANGED_FRONT_FACE_RULE = 1 << 3,
            STATE_CHANGED_COLOR_MASK = 1 << 4,
            STATE_CHANGED_DEPTH_BIAS = 1 << 5,
            STATE_CHANGED_STENCIL_MASK_FRONT = 1 << 6,
            STATE_CHANGED_BLEND_FACTOR = 1 << 7,
            STATE_CHANGED_LINE_WIDTH = 1 << 8,
            STATE_CHANGED_DEPTH_MASK = 1 << 9,
            STATE_CHANGED_DEPTH_PASS_COND = 1 << 10,
            STATE_CHANGED_DEPTH_RANGE = 1 << 11,
            STATE_CHANGED_STENCIL_FUNC_FRONT = 1 << 12,
            STATE_CHANGED_STENCIL_FUNC_BACK = 1 << 13,
            STATE_CHANGED_STENCIL_OP_FRONT = 1 << 14,
            STATE_CHANGED_STENCIL_OP_BACK = 1 << 15,
            STATE_CHANGED_STENCIL_MASK_BACK = 1 << 16
        };

        std::uint64_t non_shader_statuses_;
        std::uint64_t state_change_tracker_;
        
        std::uint8_t color_mask_;
        std::uint32_t stencil_mask_front_;
        std::uint32_t stencil_mask_back_;
        std::uint32_t depth_mask_;
        std::uint32_t depth_func_;
        std::uint32_t stencil_func_front_;
        std::uint32_t stencil_func_mask_front_;
        std::int32_t stencil_func_ref_front_;
        std::uint32_t stencil_fail_action_front_;
        std::uint32_t stencil_depth_fail_action_front_;
        std::uint32_t stencil_depth_pass_action_front_;
        std::uint32_t stencil_func_back_;
        std::uint32_t stencil_func_mask_back_;
        std::int32_t stencil_func_ref_back_;
        std::uint32_t stencil_fail_action_back_;
        std::uint32_t stencil_depth_fail_action_back_;
        std::uint32_t stencil_depth_pass_action_back_;
        
        float polygon_offset_factor_;
        float polygon_offset_units_;

        std::uint32_t pack_alignment_;
        std::uint32_t unpack_alignment_;

        float depth_range_min_;
        float depth_range_max_;

        // Vertex and index buffers
        gles_buffer_pusher vertex_buffer_pusher_;
        gles_buffer_pusher index_buffer_pusher_;
        bool attrib_changed_;

        explicit egl_context_es_shared();

        virtual gles_driver_texture *binded_texture() {
            return nullptr;
        }

        gles_driver_buffer *binded_buffer(const bool is_array_buffer);

        virtual void init_context_state() override;
        void return_handle_to_pool(const gles_object_type type, const drivers::handle h, const int subtype = 0);
        void on_surface_changed(egl_surface *prev_read, egl_surface *prev_draw) override;
        void flush_state_changes();
        
        virtual void flush_to_driver(drivers::graphics_driver *driver, const bool is_frame_swap_flush = false) override;
        virtual void destroy(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder) override;
        virtual bool prepare_for_draw(drivers::graphics_driver *driver, egl_controller &controller, kernel::process *crr_process,
            const std::int32_t first_index, const std::uint32_t vcount) = 0;
        virtual bool prepare_for_clear(drivers::graphics_driver *driver, egl_controller &controller);
        virtual bool enable(const std::uint32_t feature);
        virtual bool disable(const std::uint32_t feature);
        virtual bool is_enabled(const std::uint32_t feature, bool &enabled);
        virtual bool get_data(drivers::graphics_driver *drv, const std::uint32_t feature, void *data, gles_get_data_type data_type);
        virtual std::uint32_t bind_texture(const std::uint32_t target, const std::uint32_t tex) = 0;
    };

    egl_context_es_shared *get_es_shared_active_context(system *sys);
    void delete_gles_objects_generic(system *sys, gles_object_type obj_type, std::int32_t n, std::uint32_t *names);
}