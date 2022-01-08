/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <dispatch/libraries/gles1/gles1.h>
#include <dispatch/libraries/gles1/def.h>

#include <dispatch/dispatcher.h>
#include <drivers/graphics/graphics.h>
#include <system/epoc.h>
#include <kernel/kernel.h>

namespace eka2l1::dispatch {
    static bool convert_gl_factor_to_driver_enum(const std::uint32_t value, drivers::blend_factor &dest) {
        switch (value) {
        case GL_ONE_EMU:
            dest = drivers::blend_factor::one;
            break;

        case GL_ZERO_EMU:
            dest = drivers::blend_factor::zero;
            break;

        case GL_SRC_ALPHA_EMU:
            dest = drivers::blend_factor::frag_out_alpha;
            break;

        case GL_ONE_MINUS_SRC_ALPHA_EMU:
            dest = drivers::blend_factor::one_minus_frag_out_alpha;
            break;

        case GL_DST_ALPHA_EMU:
            dest = drivers::blend_factor::current_alpha;
            break;

        case GL_ONE_MINUS_DST_ALPHA_EMU:
            dest = drivers::blend_factor::one_minus_current_alpha;
            break;

        case GL_SRC_COLOR_EMU:
            dest = drivers::blend_factor::frag_out_color;
            break;

        case GL_ONE_MINUS_SRC_COLOR_EMU:
            dest = drivers::blend_factor::one_minus_frag_out_color;
            break;

        case GL_DST_COLOR:
            dest = drivers::blend_factor::current_color;
            break;

        case GL_ONE_MINUS_DST_COLOR:
            dest = drivers::blend_factor::one_minus_current_color;
            break;

        case GL_SRC_ALPHA_SATURATE_EMU:
            dest = drivers::blend_factor::frag_out_alpha_saturate;
            break;

        default:
            return false;
        }

        return true;
    }

    static bool cond_func_from_gl_enum(const std::uint32_t func, drivers::condition_func &drv_func) {
        switch (func) {
        case GL_ALWAYS_EMU:
            drv_func = drivers::condition_func::always;
            break;

        case GL_NEVER_EMU:
            drv_func = drivers::condition_func::never;
            break;

        case GL_GREATER_EMU:
            drv_func = drivers::condition_func::greater;
            break;

        case GL_GEQUAL_EMU:
            drv_func = drivers::condition_func::greater_or_equal;
            break;

        case GL_LESS_EMU:
            drv_func = drivers::condition_func::less;
            break;

        case GL_LEQUAL_EMU:
            drv_func = drivers::condition_func::less_or_equal;
            break;

        case GL_EQUAL_EMU:
            drv_func = drivers::condition_func::equal;
            break;

        case GL_NOTEQUAL_EMU:
            drv_func = drivers::condition_func::not_equal;
            break;

        default:
            return false;
        }

        return true;
    }

    gles_texture_unit::gles_texture_unit()
        : index_(0)
        , binded_texture_handle_(0)
        , alpha_scale_(1)
        , rgb_scale_(1) {
        texture_mat_stack_.push(glm::identity<glm::mat4>());

        env_info_.env_mode_ = gles_texture_env_info::ENV_MODE_MODULATE;
        env_colors_[0] = env_colors_[1] = env_colors_[2] = env_colors_[3] = 0.0f;
    }

    gles1_light_info::gles1_light_info()
        : spot_exponent_(0.0f)
        , spot_cutoff_(180.0f) {
        ambient_[0] = ambient_[1] = ambient_[2] = 0.0f;
        ambient_[3] = 1.0f;

        diffuse_[0] = diffuse_[1] = diffuse_[2] = diffuse_[3] = 0.0f;
        specular_[0] = specular_[1] = specular_[2] = specular_[3] = 0.0f;

        position_or_dir_[0] = position_or_dir_[1] = position_or_dir_[3] = 0.0f;
        position_or_dir_[2] = 1.0f;

        position_or_dir_transformed_[0] = position_or_dir_transformed_[1] = position_or_dir_transformed_[3] = 0.0f;
        position_or_dir_transformed_[2] = 1.0f; 

        spot_dir_[0] = spot_dir_[1] = 0.0f;
        spot_dir_[2] = -1.0f;

        spot_dir_transformed_[0] = spot_dir_transformed_[1] = 0.0f;
        spot_dir_transformed_[2] = -1.0f;

        attenuatation_[0] = 1.0f;
        attenuatation_[1] = attenuatation_[2] = 0.0f;
    }

    egl_context_es1::egl_context_es1()
        : egl_context()
        , clear_depth_(1.0f)
        , clear_stencil_(0)
        , active_texture_unit_(0)
        , active_client_texture_unit_(0)
        , active_mat_stack_(GL_MODELVIEW_EMU)
        , binded_array_buffer_handle_(0)
        , binded_element_array_buffer_handle_(0)
        , active_cull_face_(drivers::rendering_face::back)
        , active_front_face_rule_(drivers::rendering_face_determine_rule::vertices_counter_clockwise)
        , source_blend_factor_(drivers::blend_factor::one)
        , dest_blend_factor_(drivers::blend_factor::zero)
        , attrib_changed_(false)
        , material_shininess_(0.0f)
        , fog_density_(1.0f)
        , fog_start_(0.0f)
        , fog_end_(1.0f)
        , viewport_bl_(eka2l1::vec2(0, 0), eka2l1::vec2(0, 0))
        , scissor_bl_(eka2l1::vec2(0, 0), eka2l1::vec2(0, 0))
        , vertex_statuses_(0)
        , fragment_statuses_(0)
        , non_shader_statuses_(0)
        , color_mask_(0b1111)
        , stencil_mask_(0xFFFFFFFF)
        , depth_mask_(0xFFFFFFFF)
        , depth_func_(GL_LESS_EMU)
        , alpha_test_ref_(0)
        , index_buffer_temp_(0)
        , index_buffer_temp_size_(0)
        , input_desc_(0) {
        clear_color_[0] = 0.0f;
        clear_color_[1] = 0.0f;
        clear_color_[2] = 0.0f;
        clear_color_[3] = 0.0f;

        material_ambient_[0] = 0.2f;
        material_ambient_[1] = 0.2f;
        material_ambient_[2] = 0.2f;
        material_ambient_[3] = 1.0f;

        material_diffuse_[0] = 0.8f;
        material_diffuse_[1] = 0.8f;
        material_diffuse_[2] = 0.8f;
        material_diffuse_[3] = 1.0f;

        material_specular_[0] = material_specular_[1] = material_specular_[2] = material_emission_[0]
            = material_emission_[1] = material_emission_[2] = 0.0f;

        material_specular_[3] = material_emission_[3] = 1.0f;

        // Fog
        fog_color_[0] = fog_color_[1] = fog_color_[2] = fog_color_[3] = 0.0f;

        // Lights
        lights_[0].diffuse_[0] = lights_[0].diffuse_[1] = lights_[0].diffuse_[2] = lights_[0].diffuse_[3] = 1.0f;
        lights_[0].specular_[0] = lights_[0].specular_[1] = lights_[0].specular_[2] = lights_[0].specular_[3] = 1.0f;

        global_ambient_[0] = global_ambient_[1] = global_ambient_[2] = 0.2f;
        global_ambient_[3] = 1.0f;

        // Clip planes
        for (std::size_t i = 0; i < GLES1_EMU_MAX_CLIP_PLANE; i++) {
            clip_planes_[i][0] = clip_planes_[i][1] = clip_planes_[i][2] = clip_planes_[i][3] = 0.0f;
            clip_planes_transformed_[i][0] = clip_planes_transformed_[i][1] = clip_planes_transformed_[i][2] = clip_planes_transformed_[i][3] = 0.0f;
        }

        // Uniforms
        color_uniforms_[0] = color_uniforms_[1] = color_uniforms_[2] = color_uniforms_[3] = 1.0f;

        // Attribs
        vertex_attrib_.size_ = 4;
        vertex_attrib_.stride_ = 0;
        vertex_attrib_.offset_ = 0;
        vertex_attrib_.data_type_ = GL_FLOAT_EMU;

        for (std::size_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
            texture_units_[i].coord_attrib_ = vertex_attrib_;
        }

        normal_attrib_ = vertex_attrib_;
        normal_attrib_.size_ = 3;

        color_attrib_ = vertex_attrib_;
        color_attrib_.size_ = 4;

        // Matrix
        model_view_mat_stack_.push(glm::identity<glm::mat4>());
        proj_mat_stack_.push(glm::identity<glm::mat4>());

        for (std::size_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
            texture_units_[i].index_ = i;
        }

        fragment_statuses_ |= ((GL_ALWAYS_EMU - GL_NEVER_EMU) << FRAGMENT_STATE_ALPHA_TEST_FUNC_POS) & FRAGMENT_STATE_ALPHA_FUNC_MASK;
        fragment_statuses_ |= FRAGMENT_STATE_FOG_MODE_EXP;
    }

    void egl_context_es1::free(drivers::graphics_driver *driver, drivers::graphics_command_list_builder &builder) {
        if (index_buffer_temp_) {
            builder.destroy(index_buffer_temp_);
        }

        if (input_desc_) {
            builder.destroy(input_desc_);
        }

        if (vertex_attrib_.in_house_buffer_) {
            builder.destroy(vertex_attrib_.in_house_buffer_);
        }
        
        if (color_attrib_.in_house_buffer_) {
            builder.destroy(color_attrib_.in_house_buffer_);
        }
        
        if (normal_attrib_.in_house_buffer_) {
            builder.destroy(normal_attrib_.in_house_buffer_);
        }

        for (int i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
            if (texture_units_[i].coord_attrib_.in_house_buffer_) {
                builder.destroy(texture_units_[i].coord_attrib_.in_house_buffer_);
            }
        }

        egl_context::free(driver, builder);
    }

    void egl_context_es1::init_context_state(drivers::graphics_command_list_builder &builder) {
        builder.bind_bitmap(draw_surface_->handle_, read_surface_->handle_);
        builder.set_cull_face(active_cull_face_);
        builder.set_front_face_rule(active_front_face_rule_);
        builder.set_color_mask(color_mask_);
        builder.set_stencil_mask(drivers::rendering_face::back_and_front, stencil_mask_);

        drivers::condition_func func;
        cond_func_from_gl_enum(depth_func_, func);

        builder.set_depth_pass_condition(func);
        builder.set_depth_mask(depth_mask_);
        builder.blend_formula(drivers::blend_equation::add, drivers::blend_equation::add, source_blend_factor_, dest_blend_factor_,
            source_blend_factor_, dest_blend_factor_);

        builder.set_feature(drivers::graphics_feature::blend, non_shader_statuses_ & NON_SHADER_STATE_BLEND_ENABLE);
        builder.set_feature(drivers::graphics_feature::clipping, non_shader_statuses_ & NON_SHADER_STATE_SCISSOR_ENABLE);
        builder.set_feature(drivers::graphics_feature::cull, non_shader_statuses_ & NON_SHADER_STATE_CULL_FACE_ENABLE);
        builder.set_feature(drivers::graphics_feature::depth_test, non_shader_statuses_ & NON_SHADER_STATE_DEPTH_TEST_ENABLE);
        builder.set_feature(drivers::graphics_feature::dither, non_shader_statuses_ & NON_SHADER_STATE_DITHER);
        builder.set_feature(drivers::graphics_feature::line_smooth, non_shader_statuses_ & NON_SHADER_STATE_LINE_SMOOTH);
        builder.set_feature(drivers::graphics_feature::multisample, non_shader_statuses_ & NON_SHADER_STATE_MULTISAMPLE);
        builder.set_feature(drivers::graphics_feature::polygon_offset_fill, non_shader_statuses_ & NON_SHADER_STATE_POLYGON_OFFSET_FILL);
        builder.set_feature(drivers::graphics_feature::sample_alpha_to_coverage, non_shader_statuses_ & NON_SHADER_STATE_SAMPLE_ALPHA_TO_COVERAGE);
        builder.set_feature(drivers::graphics_feature::sample_alpha_to_one, non_shader_statuses_ & NON_SHADER_STATE_SAMPLE_ALPHA_TO_ONE);
        builder.set_feature(drivers::graphics_feature::sample_coverage, non_shader_statuses_ & NON_SHADER_STATE_SAMPLE_COVERAGE);
        builder.set_feature(drivers::graphics_feature::stencil_test, non_shader_statuses_ & NON_SHADER_STATE_STENCIL_TEST_ENABLE);

        builder.clip_rect(eka2l1::rect(eka2l1::vec2(scissor_bl_.top.x, draw_surface_->dimension_.y - (scissor_bl_.top.y + scissor_bl_.size.y)),
            scissor_bl_.size));

        builder.set_viewport(eka2l1::rect(eka2l1::vec2(viewport_bl_.top.x, draw_surface_->dimension_.y - (viewport_bl_.top.y + viewport_bl_.size.y)),
            viewport_bl_.size));
    }

    glm::mat4 &egl_context_es1::active_matrix() {
        switch (active_mat_stack_) {
        case GL_MODELVIEW_EMU:
            return model_view_mat_stack_.top();

        case GL_PROJECTION_EMU:
            return proj_mat_stack_.top();

        case GL_TEXTURE_EMU:
            return texture_units_[active_texture_unit_].texture_mat_stack_.top();

        default:
            break;
        }

        return model_view_mat_stack_.top();
    }

    gles1_driver_texture *egl_context_es1::binded_texture() {
        if (texture_units_[active_texture_unit_].binded_texture_handle_ == 0) {
            return nullptr;
        }

        auto *obj = objects_.get(texture_units_[active_texture_unit_].binded_texture_handle_);
        if (!obj || (*obj)->object_type() != GLES1_OBJECT_TEXTURE) {
            return nullptr;
        }

        return reinterpret_cast<gles1_driver_texture*>(obj->get());
    }

    drivers::handle egl_context_es1::binded_texture_driver_handle() {
        if (!texture_units_[active_texture_unit_].binded_texture_handle_) {
            return draw_surface_->handle_;
        }

        auto tex = binded_texture();
        if (tex) {
            return tex->handle_value();
        }

        return 0;
    }

    gles1_driver_buffer *egl_context_es1::binded_buffer(const bool is_array_buffer) {
        auto *obj = objects_.get(is_array_buffer ? binded_array_buffer_handle_ : binded_element_array_buffer_handle_);
        if (!obj || (*obj)->object_type() != GLES1_OBJECT_BUFFER) {
            return nullptr;
        }

        return reinterpret_cast<gles1_driver_buffer*>(obj->get());
    }

    void egl_context_es1::return_handle_to_pool(const gles1_object_type type, const drivers::handle h) {
        switch (type) {
        case GLES1_OBJECT_BUFFER:
            buffer_pools_.push(h);
            break;

        case GLES1_OBJECT_TEXTURE:
            texture_pools_.push(h);
            break;

        default:
            break;
        }
    }

    gles1_driver_object::gles1_driver_object(egl_context_es1 *ctx)
        : context_(ctx)
        , driver_handle_(0) {
    }

    gles1_driver_object::~gles1_driver_object() {
        if (driver_handle_ != 0) {
            if (context_) {
                context_->return_handle_to_pool(object_type(), driver_handle_);
            }
        }
    }

    gles1_driver_texture::gles1_driver_texture(egl_context_es1 *ctx)
        : gles1_driver_object(ctx)
        , internal_format_(0)
        , min_filter_(GL_NEAREST_MIPMAP_LINEAR_EMU)
        , mag_filter_(GL_LINEAR_EMU)
        , wrap_s_(GL_REPEAT_EMU)
        , wrap_t_(GL_REPEAT_EMU)
        , auto_regen_mipmap_(false) {
    }

    gles1_driver_buffer::gles1_driver_buffer(egl_context_es1 *ctx)
        : gles1_driver_object(ctx)
        , data_size_(0) {
    }

    egl_context_es1 *get_es1_active_context(system *sys) {
        if (!sys) {
            return nullptr;
        }

        dispatcher *dp = sys->get_dispatcher();
        if (!dp) {
            return nullptr;
        }

        kernel_system *kern = sys->get_kernel_system();
        if (!kern) {
            return nullptr;
        }

        kernel::thread *crr_thread = kern->crr_thread();
        if (!crr_thread) {
            return nullptr;
        }

        dispatch::egl_controller &controller = dp->get_egl_controller();
        kernel::uid crr_thread_uid = crr_thread->unique_id();

        egl_context *context = controller.current_context(crr_thread_uid);
        if (!context || (context->context_type() != EGL_GLES1_CONTEXT)) {
            controller.push_error(crr_thread_uid, GL_INVALID_OPERATION);
            return nullptr;
        }

        return reinterpret_cast<egl_context_es1*>(context);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_clear_color_emu, float red, float green, float blue, float alpha) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->clear_color_[0] = red;
        ctx->clear_color_[1] = green;
        ctx->clear_color_[2] = blue;
        ctx->clear_color_[3] = alpha;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_clear_colorx_emu, gl_fixed red, gl_fixed green, gl_fixed blue, gl_fixed alpha) {
        gl_clear_color_emu(sys, FIXED_32_TO_FLOAT(red), FIXED_32_TO_FLOAT(green), FIXED_32_TO_FLOAT(blue), FIXED_32_TO_FLOAT(alpha));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_clear_depthf_emu, float depth) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->clear_depth_ = depth;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_clear_depthx_emu, gl_fixed depth) {
        gl_clear_depthf_emu(sys, FIXED_32_TO_FLOAT(depth));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_clear_stencil, std::int32_t s) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->clear_stencil_ = s;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_clear_emu, std::uint32_t bits) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        eka2l1::vecx<float, 6> clear_parameters;

        std::uint32_t flags_driver = 0;
        if (bits & GL_COLOR_BUFFER_BIT_EMU) {
            flags_driver |= drivers::draw_buffer_bit_color_buffer;

            clear_parameters[0] = ctx->clear_color_[0];
            clear_parameters[1] = ctx->clear_color_[1];
            clear_parameters[2] = ctx->clear_color_[2];
            clear_parameters[3] = ctx->clear_color_[3];
        }

        if (bits & GL_DEPTH_BUFFER_BIT_EMU) {
            flags_driver |= drivers::draw_buffer_bit_depth_buffer;
            clear_parameters[4] = ctx->clear_depth_;
        }

        if (bits & GL_STENCIL_BUFFER_BIT_EMU) {
            flags_driver |= drivers::draw_buffer_bit_stencil_buffer;
            clear_parameters[5] = ctx->clear_stencil_ / 255.0f;
        }

        ctx->command_builder_->clear(clear_parameters, flags_driver);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_matrix_mode_emu, std::uint32_t mode) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();

        if ((mode != GL_TEXTURE_EMU) && (mode != GL_MODELVIEW_EMU) && (mode != GL_PROJECTION_EMU)) {
            dp->get_egl_controller().push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        ctx->active_mat_stack_ = mode;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_push_matrix_emu) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (ctx->active_mat_stack_) {
        case GL_MODELVIEW_EMU:
            if (ctx->model_view_mat_stack_.size() >= GL_MAXIMUM_MODELVIEW_MATRIX_STACK_SIZE) {
                controller.push_error(ctx, GL_STACK_OVERFLOW);
                return;
            }

            ctx->model_view_mat_stack_.push(ctx->model_view_mat_stack_.top());
            break;

        case GL_PROJECTION_EMU:
            if (ctx->proj_mat_stack_.size() >= GL_MAXIMUM_PROJECTION_MATRIX_STACK_SIZE) {
                controller.push_error(ctx, GL_STACK_OVERFLOW);
                return;
            }

            ctx->proj_mat_stack_.push(ctx->proj_mat_stack_.top());
            break;

        case GL_TEXTURE_EMU: {
            auto &ss = ctx->texture_units_[ctx->active_texture_unit_].texture_mat_stack_;

            if (ss.size() >= GL_MAXIMUM_TEXTURE_MATRIX_STACK_SIZE) {
                controller.push_error(ctx, GL_STACK_OVERFLOW);
                return;
            }

            ss.push(ss.top());
            break;
        }
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_pop_matrix_emu) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (ctx->active_mat_stack_) {
        case GL_MODELVIEW_EMU:
            if (ctx->model_view_mat_stack_.size() == 1) {
                controller.push_error(ctx, GL_STACK_UNDERFLOW);
                return;
            }

            ctx->model_view_mat_stack_.pop();
            break;

        case GL_PROJECTION_EMU:
            if (ctx->proj_mat_stack_.size() == 1) {
                controller.push_error(ctx, GL_STACK_UNDERFLOW);
                return;
            }

            ctx->proj_mat_stack_.pop();
            break;

        case GL_TEXTURE_EMU:
            if (ctx->texture_units_[ctx->active_texture_unit_].texture_mat_stack_.size() == 1) {
                controller.push_error(ctx, GL_STACK_UNDERFLOW);
                return;
            }

            ctx->texture_units_[ctx->active_texture_unit_].texture_mat_stack_.pop();
            break;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_load_identity_emu) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::identity<glm::mat4>();
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_load_matrixf_emu, float *mat) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!mat) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        // Column wise, so is good
        ctx->active_matrix() = glm::make_mat4(mat);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_load_matrixx_emu, gl_fixed *mat) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!mat) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        float converted[16];
        for (int i = 0; i < 16; i++) {
            converted[i] = FIXED_32_TO_FLOAT(mat[i]);
        }

        // Column wise, so is good
        ctx->active_matrix() = glm::make_mat4(converted);
    }
    
    BRIDGE_FUNC_LIBRARY(std::uint32_t, gl_get_error_emu) {
        dispatcher *dp = sys->get_dispatcher();
        kernel_system *kern = sys->get_kernel_system();

        if (!dp || !kern) {
            return GL_INVALID_OPERATION;
        }

        kernel::thread *crr_thread = kern->crr_thread();
        if (!crr_thread) {
            return GL_INVALID_OPERATION;
        }

        dispatch::egl_controller &controller = dp->get_egl_controller();
        return controller.pop_error(crr_thread->unique_id());
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_orthof_emu, float left, float right, float bottom, float top, float near, float far) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() *= glm::ortho(left, right, bottom, top, near, far);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_orthox_emu, gl_fixed left, gl_fixed right, gl_fixed bottom, gl_fixed top, gl_fixed near, gl_fixed far) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() *= glm::ortho(FIXED_32_TO_FLOAT(left), FIXED_32_TO_FLOAT(right), FIXED_32_TO_FLOAT(bottom), FIXED_32_TO_FLOAT(top),
            FIXED_32_TO_FLOAT(near), FIXED_32_TO_FLOAT(far));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_mult_matrixf_emu, float *m) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!m) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        ctx->active_matrix() *= glm::make_mat4(m);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_mult_matrixx_emu, gl_fixed *m) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!m) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        float converted[16];
        for (int i = 0; i < 16; i++) {
            converted[i] = FIXED_32_TO_FLOAT(m[i]);
        }

        ctx->active_matrix() *= glm::make_mat4(converted);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_scalef_emu, float x, float y, float z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::scale(ctx->active_matrix(), glm::vec3(x, y, z));
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_scalex_emu, gl_fixed x, gl_fixed y, gl_fixed z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::scale(ctx->active_matrix(), glm::vec3(FIXED_32_TO_FLOAT(x), FIXED_32_TO_FLOAT(y), FIXED_32_TO_FLOAT(z)));
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_translatef_emu, float x, float y, float z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::translate(ctx->active_matrix(), glm::vec3(x, y, z));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_translatex_emu, gl_fixed x, gl_fixed y, gl_fixed z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::translate(ctx->active_matrix(), glm::vec3(FIXED_32_TO_FLOAT(x), FIXED_32_TO_FLOAT(y), FIXED_32_TO_FLOAT(z)));
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_rotatef_emu, float angles, float x, float y, float z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::rotate(ctx->active_matrix(), glm::radians(angles), glm::vec3(x, y, z));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_rotatex_emu, gl_fixed angles, gl_fixed x, gl_fixed y, gl_fixed z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::rotate(ctx->active_matrix(), glm::radians(FIXED_32_TO_FLOAT(angles)), glm::vec3(FIXED_32_TO_FLOAT(x), FIXED_32_TO_FLOAT(y), FIXED_32_TO_FLOAT(z)));
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_frustumf_emu, float left, float right, float bottom, float top, float near, float far) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() *= glm::frustum(left, right, bottom, top, near, far);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_frustumx_emu, gl_fixed left, gl_fixed right, gl_fixed bottom, gl_fixed top, gl_fixed near, gl_fixed far) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() *= glm::frustum(FIXED_32_TO_FLOAT(left), FIXED_32_TO_FLOAT(right), FIXED_32_TO_FLOAT(bottom), FIXED_32_TO_FLOAT(top),
            FIXED_32_TO_FLOAT(near), FIXED_32_TO_FLOAT(far));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_cull_face_emu, std::uint32_t mode) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (mode) {
        case GL_BACK_EMU:
            ctx->active_cull_face_ = drivers::rendering_face::back;
            break;

        case GL_FRONT_EMU:
            ctx->active_cull_face_ = drivers::rendering_face::front;
            break;

        case GL_FRONT_AND_BACK_EMU:
            ctx->active_cull_face_ = drivers::rendering_face::back_and_front;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        ctx->command_builder_->set_cull_face(ctx->active_cull_face_);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_scissor_emu, std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        // Emulator graphics abstraction use top-left as origin.
        ctx->scissor_bl_.top = eka2l1::vec2(x, y);
        ctx->scissor_bl_.size = eka2l1::vec2(width, height);

        ctx->command_builder_->clip_rect(eka2l1::rect(eka2l1::vec2(x, ctx->draw_surface_->dimension_.y - (y + height)), eka2l1::vec2(width, height)));
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_front_face_emu, std::uint32_t mode) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (mode) {
        case GL_CLOCKWISE_EMU:
            ctx->active_front_face_rule_ = drivers::rendering_face_determine_rule::vertices_clockwise;
            break;

        case GL_COUNTER_CLOCKWISE_EMU:
            ctx->active_front_face_rule_ = drivers::rendering_face_determine_rule::vertices_counter_clockwise;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        ctx->command_builder_->set_front_face_rule(ctx->active_front_face_rule_);
    }
    
    BRIDGE_FUNC_LIBRARY(bool, gl_is_texture_emu, std::uint32_t name) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return false;
        }

        gles1_driver_object_instance *obj = ctx->objects_.get(name);
        return obj && ((*obj)->object_type() == GLES1_OBJECT_TEXTURE);
    }

    BRIDGE_FUNC_LIBRARY(bool, gl_is_buffer_emu, std::uint32_t name) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return false;
        }

        gles1_driver_object_instance *obj = ctx->objects_.get(name);
        return obj && ((*obj)->object_type() == GLES1_OBJECT_BUFFER);
    }
    
    static void gen_gles1_objects_generic(system *sys, gles1_object_type obj_type, std::int32_t n, std::uint32_t *texs) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (n < 0 || !texs) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (n == 0) {
            return;
        }

        gles1_driver_object_instance stub_obj;

        for (std::int32_t i = 0; i < n; i++) {
            switch (obj_type) {
            case GLES1_OBJECT_TEXTURE:
                stub_obj = std::make_unique<gles1_driver_texture>(ctx);
                break;

            case GLES1_OBJECT_BUFFER:
                stub_obj = std::make_unique<gles1_driver_buffer>(ctx);
                break;

            default:
                break;
            }
            texs[i] = static_cast<std::uint32_t>(ctx->objects_.add(stub_obj));
        }
    }

    static void delete_gles1_objects_generic(system *sys, gles1_object_type obj_type, std::int32_t n, std::uint32_t *names) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        drivers::graphics_driver *drv = sys->get_graphics_driver();

        if (n < 0 || !names) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (n == 0) {
            return;
        }

        for (std::int32_t i = 0; i < n; i++) {
            auto *obj = ctx->objects_.get(names[i]);
            if (obj && ((*obj)->object_type() == obj_type)) {
                ctx->objects_.remove(names[i]);
            }
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_gen_textures_emu, std::int32_t n, std::uint32_t *texs) {
        gen_gles1_objects_generic(sys, GLES1_OBJECT_TEXTURE, n, texs);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_gen_buffers_emu, std::int32_t n, std::uint32_t *buffers) {
        gen_gles1_objects_generic(sys, GLES1_OBJECT_BUFFER, n, buffers);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_delete_textures_emu, std::int32_t n, std::uint32_t *texs) {
        delete_gles1_objects_generic(sys, GLES1_OBJECT_TEXTURE, n, texs);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_delete_buffers_emu, std::int32_t n, std::uint32_t *buffers) {
        delete_gles1_objects_generic(sys, GLES1_OBJECT_BUFFER, n, buffers);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_bind_texture_emu, std::uint32_t target, std::uint32_t name) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_TEXTURE_2D_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->texture_units_[ctx->active_texture_unit_].binded_texture_handle_ = name;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_bind_buffer_emu, std::uint32_t target, std::uint32_t name) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (target) {
        case GL_ARRAY_BUFFER_EMU:
            ctx->binded_array_buffer_handle_ = name;
            break;

        case GL_ELEMENT_ARRAY_BUFFER_EMU:
            ctx->binded_element_array_buffer_handle_ = name;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    static inline std::uint32_t is_format_and_data_type_ok(const std::uint32_t format, const std::uint32_t data_type) {
        if ((format != GL_ALPHA_EMU) && (format != GL_RGB_EMU) && (format != GL_RGBA_EMU) && (format != GL_LUMINANCE_EMU)
            && (format != GL_LUMINANCE_ALPHA_EMU)) {
            return GL_INVALID_ENUM;
        }

        if ((data_type != GL_UNSIGNED_BYTE_EMU) && (data_type != GL_UNSIGNED_SHORT_4_4_4_4_EMU) && (data_type != GL_UNSIGNED_SHORT_5_5_5_1_EMU)
            && (data_type != GL_UNSIGNED_SHORT_5_6_5_EMU)) {
            return GL_INVALID_ENUM;
        }

        if ((data_type == GL_UNSIGNED_SHORT_5_6_5_EMU) && (format != GL_RGB_EMU)) {
            return GL_INVALID_OPERATION;
        }

        if (((data_type == GL_UNSIGNED_SHORT_5_5_5_1_EMU) || (data_type == GL_UNSIGNED_SHORT_4_4_4_4_EMU)) && (format != GL_RGBA_EMU)) {
            return GL_INVALID_OPERATION;
        }

        return 0;
    }

    static bool is_valid_gl_emu_func(const std::uint32_t func) {
        return ((func == GL_NEVER_EMU) || (func == GL_ALWAYS_EMU) || (func == GL_GREATER_EMU) || (func == GL_GEQUAL_EMU) ||
            (func == GL_LESS_EMU) || (func == GL_LEQUAL_EMU) || (func == GL_EQUAL_EMU) || (func == GL_NOTEQUAL_EMU));
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_alpha_func_emu, std::uint32_t func, float ref) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!is_valid_gl_emu_func(func)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_ALPHA_FUNC_MASK;
        ctx->fragment_statuses_ |= ((func - GL_NEVER_EMU) << egl_context_es1::FRAGMENT_STATE_ALPHA_TEST_FUNC_POS)
            & egl_context_es1::FRAGMENT_STATE_ALPHA_FUNC_MASK;

        ctx->alpha_test_ref_ = ref;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_alpha_func_x_emu, std::uint32_t func, gl_fixed ref) {
        gl_alpha_func_emu(sys, func, FIXED_32_TO_FLOAT(ref));
    }

    // This will only works on GLES1, they don't have swizzle masks yet.
    static void get_data_type_to_upload(drivers::texture_format &internal_out, drivers::texture_format &format_out, drivers::texture_data_type &type_out,
        drivers::channel_swizzles &swizzles_out, const std::uint32_t format, const std::uint32_t data_type) {
        switch (format) {
        case GL_RGB_EMU:
            format_out = drivers::texture_format::rgb;
            internal_out = format_out;
            swizzles_out = { drivers::channel_swizzle::red, drivers::channel_swizzle::green, drivers::channel_swizzle::blue, drivers::channel_swizzle::one };
            break;

        case GL_RGBA_EMU:
            format_out = drivers::texture_format::rgba;
            internal_out = format_out;
            swizzles_out = { drivers::channel_swizzle::red, drivers::channel_swizzle::green, drivers::channel_swizzle::blue, drivers::channel_swizzle::alpha };
            break;

        case GL_ALPHA_EMU:
            format_out = drivers::texture_format::r;
            internal_out = drivers::texture_format::r8;
            swizzles_out = { drivers::channel_swizzle::zero, drivers::channel_swizzle::zero, drivers::channel_swizzle::zero, drivers::channel_swizzle::red };
            break;

        case GL_LUMINANCE_EMU:
            format_out = drivers::texture_format::r;
            internal_out = drivers::texture_format::r8;
            swizzles_out = { drivers::channel_swizzle::red, drivers::channel_swizzle::red, drivers::channel_swizzle::red, drivers::channel_swizzle::one };
            break;
            
        case GL_LUMINANCE_ALPHA_EMU:
            format_out = drivers::texture_format::rg;
            internal_out = drivers::texture_format::rg8;
            swizzles_out = { drivers::channel_swizzle::red, drivers::channel_swizzle::red, drivers::channel_swizzle::red, drivers::channel_swizzle::green };
            break;

        default:
            break;
        }

        switch (data_type) {
        case GL_UNSIGNED_BYTE_EMU:
            type_out = drivers::texture_data_type::ubyte;
            break;

        case GL_UNSIGNED_SHORT_4_4_4_4_EMU:
            type_out = drivers::texture_data_type::ushort_4_4_4_4;
            break;

        case GL_UNSIGNED_SHORT_5_5_5_1_EMU:
            type_out = drivers::texture_data_type::ushort_5_5_5_1;
            break;

        case GL_UNSIGNED_SHORT_5_6_5_EMU:
            type_out = drivers::texture_data_type::ushort_5_6_5;
            break;

        default:
            break;
        }
    }

    static std::uint32_t calculate_possible_upload_size(const eka2l1::vec2 size, const std::uint32_t format, const std::uint32_t data_type) {
        if ((format == GL_LUMINANCE_ALPHA_EMU) || (format == GL_LUMINANCE_EMU) || (format == GL_ALPHA_EMU)) {
            return size.x * size.y;
        }

        if (format == GL_RGB_EMU) {
            if (data_type == GL_UNSIGNED_BYTE_EMU) {
                return size.x * size.y * 3;
            } else {
                return size.x * size.y * 2;
            }
        } else {
            if (format == GL_RGBA_EMU) {
                if (data_type == GL_UNSIGNED_BYTE_EMU) {
                    return size.x * size.y * 4;
                } else {
                    return size.x * size.y * 2;
                }
            }
        }

        return 0;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t internal_format,
        std::int32_t width, std::int32_t height, std::int32_t border, std::uint32_t format, std::uint32_t data_type,
        void *data_pixels) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_TEXTURE_2D_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if ((level < 0) || (level > GLES1_EMU_MAX_TEXTURE_MIP_LEVEL) || (width < 0) || (height < 0) || (width > GLES1_EMU_MAX_TEXTURE_SIZE)
            || (height > GLES1_EMU_MAX_TEXTURE_SIZE) || !data_pixels) {
            controller.push_error(ctx, GL_INVALID_VALUE);
        }

        if (internal_format != format) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        const std::uint32_t error = is_format_and_data_type_ok(format, data_type);
        if (error != 0) {
            controller.push_error(ctx, error);
            return;
        }

        gles1_driver_texture *tex = ctx->binded_texture();
        if (!tex) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        bool need_reinstantiate = true;

        drivers::texture_format internal_format_driver;
        drivers::texture_format format_driver;
        drivers::texture_data_type dtype;
        drivers::channel_swizzles swizzles;

        get_data_type_to_upload(internal_format_driver, format_driver, dtype, swizzles, format, data_type);

        // TODO: border is ignored!
        if (!tex->handle_value()) {
            if (!ctx->texture_pools_.empty()) {
                tex->assign_handle(ctx->texture_pools_.top());
                ctx->texture_pools_.pop();
            } else {
                drivers::graphics_driver *drv = sys->get_graphics_driver();
                drivers::handle new_h = drivers::create_texture(drv, 2, static_cast<std::uint8_t>(level), internal_format_driver,
                    format_driver, dtype, data_pixels, eka2l1::vec3(width, height, 0));

                if (!new_h) {
                    controller.push_error(ctx, GL_INVALID_OPERATION);
                    return;
                }

                need_reinstantiate = false;
                tex->assign_handle(new_h);
            }
        }

        if (need_reinstantiate) {
            const std::size_t needed_size = calculate_possible_upload_size(eka2l1::vec2(width, height), format, data_type);
            ctx->command_builder_->recreate_texture(tex->handle_value(), 2, static_cast<std::uint8_t>(level), internal_format_driver,
                format_driver, dtype, data_pixels, needed_size, eka2l1::vec3(width, height, 0));
        }

        tex->set_internal_format(format);
        tex->set_size(eka2l1::vec2(width, height));

        if (tex->auto_regenerate_mipmap()) {
            ctx->command_builder_->regenerate_mips(tex->handle_value());
        }

        ctx->command_builder_->set_swizzle(tex->handle_value(), swizzles[0], swizzles[1], swizzles[2], swizzles[3]);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_tex_sub_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t xoffset,
        std::int32_t yoffset, std::int32_t width, std::int32_t height, std::uint32_t format, std::uint32_t data_type,
        void *data_pixels) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_TEXTURE_2D_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if ((level < 0) || (level > GLES1_EMU_MAX_TEXTURE_MIP_LEVEL) || (width < 0) || (height < 0) || (width > GLES1_EMU_MAX_TEXTURE_SIZE)
            || (height > GLES1_EMU_MAX_TEXTURE_SIZE) || !data_pixels) {
            controller.push_error(ctx, GL_INVALID_VALUE);
        }

        const std::uint32_t error = is_format_and_data_type_ok(format, data_type);
        if (error != 0) {
            controller.push_error(ctx, error);
            return;
        }

        gles1_driver_texture *tex = ctx->binded_texture();
        if (!tex || !tex->handle_value()) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        if ((xoffset < 0) || (yoffset < 0) || (xoffset + width > tex->size().x) || (yoffset + height > tex->size().y)) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        // On GLES3 this is true, unsure with GLES1, but still put here for our ease.
        if (tex->internal_format() != format) {
            LOG_ERROR(HLE_GLES1, "Internal format and format passed to texture update does not match!");
            controller.push_error(ctx, GL_INVALID_OPERATION);

            return;
        }

        bool need_reinstantiate = true;

        drivers::texture_format internal_format_driver;
        drivers::texture_format format_driver;
        drivers::texture_data_type dtype;
        drivers::channel_swizzles swizzles;

        get_data_type_to_upload(internal_format_driver, format_driver, dtype, swizzles, format, data_type);

        const std::size_t needed_size = calculate_possible_upload_size(eka2l1::vec2(width, height), format, data_type);
        ctx->command_builder_->update_texture(tex->handle_value(), reinterpret_cast<const char*>(data_pixels), needed_size, format_driver,
            dtype, eka2l1::vec3(xoffset, yoffset, 0), eka2l1::vec3(width, height, 0));
   
        if (tex->auto_regenerate_mipmap()) {
            ctx->command_builder_->regenerate_mips(tex->handle_value());
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_shade_model_emu, std::uint32_t model) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (model == GL_FLAT_EMU) {
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_SHADE_MODEL_FLAT;
        } else if (model == GL_SMOOTH_EMU) {
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_SHADE_MODEL_FLAT;
        } else {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_normal_3f_emu, float nx, float ny, float nz) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->normal_uniforms_[0] = nx;
        ctx->normal_uniforms_[1] = ny;
        ctx->normal_uniforms_[2] = nz;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_normal_3x_emu, gl_fixed nx, gl_fixed ny, gl_fixed nz) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->normal_uniforms_[0] = FIXED_32_TO_FLOAT(nx);
        ctx->normal_uniforms_[1] = FIXED_32_TO_FLOAT(ny);
        ctx->normal_uniforms_[2] = FIXED_32_TO_FLOAT(nz);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_color_4f_emu, float red, float green, float blue, float alpha) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->color_uniforms_[0] = red;
        ctx->color_uniforms_[1] = green;
        ctx->color_uniforms_[2] = blue;
        ctx->color_uniforms_[3] = alpha;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_color_4x_emu, gl_fixed red, gl_fixed green, gl_fixed blue, gl_fixed alpha) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->color_uniforms_[0] = FIXED_32_TO_FLOAT(red);
        ctx->color_uniforms_[1] = FIXED_32_TO_FLOAT(green);
        ctx->color_uniforms_[2] = FIXED_32_TO_FLOAT(blue);
        ctx->color_uniforms_[3] = FIXED_32_TO_FLOAT(alpha);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_color_4ub_emu, std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->color_uniforms_[0] = red / 255.0f;
        ctx->color_uniforms_[1] = green / 255.0f;
        ctx->color_uniforms_[2] = blue / 255.0f;
        ctx->color_uniforms_[3] = alpha / 255.0f;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_active_texture_emu, std::uint32_t unit) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((unit < GL_TEXTURE0_EMU) && (unit >= GL_TEXTURE0_EMU + GLES1_EMU_MAX_TEXTURE_COUNT)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->active_texture_unit_ = unit - GL_TEXTURE0_EMU;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_client_active_texture_emu, std::uint32_t unit) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((unit < GL_TEXTURE0_EMU) && (unit >= GL_TEXTURE0_EMU + GLES1_EMU_MAX_TEXTURE_COUNT)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->active_client_texture_unit_ = unit - GL_TEXTURE0_EMU;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_multi_tex_coord_4f_emu, std::uint32_t unit, float s, float t, float r, float q) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((unit < GL_TEXTURE0_EMU) && (unit >= GL_TEXTURE0_EMU + GLES1_EMU_MAX_TEXTURE_COUNT)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->texture_units_[unit].coord_uniforms_[0] = s;
        ctx->texture_units_[unit].coord_uniforms_[1] = t;
        ctx->texture_units_[unit].coord_uniforms_[2] = r;
        ctx->texture_units_[unit].coord_uniforms_[3] = q;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_multi_tex_coord_4x_emu, std::uint32_t unit, gl_fixed s, gl_fixed t, gl_fixed r, gl_fixed q) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((unit < GL_TEXTURE0_EMU) && (unit >= GL_TEXTURE0_EMU + GLES1_EMU_MAX_TEXTURE_COUNT)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->texture_units_[unit].coord_uniforms_[0] = FIXED_32_TO_FLOAT(s);
        ctx->texture_units_[unit].coord_uniforms_[1] = FIXED_32_TO_FLOAT(t);
        ctx->texture_units_[unit].coord_uniforms_[2] = FIXED_32_TO_FLOAT(r);
        ctx->texture_units_[unit].coord_uniforms_[3] = FIXED_32_TO_FLOAT(q);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_enable_client_state_emu, std::uint32_t state) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (state) {
        case GL_VERTEX_ARRAY_EMU:
            ctx->vertex_statuses_ |= egl_context_es1::VERTEX_STATE_CLIENT_VERTEX_ARRAY;
            break;

        case GL_COLOR_ARRAY_EMU:
            ctx->vertex_statuses_ |= egl_context_es1::VERTEX_STATE_CLIENT_COLOR_ARRAY;
            break;

        case GL_NORMAL_ARRAY_EMU:
            ctx->vertex_statuses_ |= egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY;
            break;

        case GL_TEXTURE_COORD_ARRAY_EMU:
            ctx->vertex_statuses_ |= egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD_ARRAY;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_disable_client_state_emu, std::uint32_t state) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (state) {
        case GL_VERTEX_ARRAY_EMU:
            ctx->vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_CLIENT_VERTEX_ARRAY;
            break;

        case GL_COLOR_ARRAY_EMU:
            ctx->vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_CLIENT_COLOR_ARRAY;
            break;

        case GL_NORMAL_ARRAY_EMU:
            ctx->vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY;
            break;

        case GL_TEXTURE_COORD_ARRAY_EMU:
            ctx->vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD_ARRAY;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_buffer_data_emu, std::uint32_t target, std::int32_t size, const void *data, std::uint32_t usage) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        gles1_driver_buffer *buffer = ctx->binded_buffer(target == GL_ARRAY_BUFFER_EMU);
        if (!buffer) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        if (size < 0) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        drivers::buffer_upload_hint upload_hint;

        switch (usage) {
        case GL_STATIC_DRAW_EMU:
            upload_hint = static_cast<drivers::buffer_upload_hint>(drivers::buffer_upload_static | drivers::buffer_upload_draw);
            break;

        case GL_DYNAMIC_DRAW_EMU:
            upload_hint = static_cast<drivers::buffer_upload_hint>(drivers::buffer_upload_dynamic | drivers::buffer_upload_draw);
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        bool need_reinstantiate = true;

        if (buffer->handle_value() == 0) {
            if (!ctx->buffer_pools_.empty()) {
                buffer->assign_handle(ctx->buffer_pools_.top());
                ctx->buffer_pools_.pop();
            } else {
                drivers::graphics_driver *drv = sys->get_graphics_driver();
                drivers::handle new_h = drivers::create_buffer(drv, data, size, upload_hint);

                if (!new_h) {
                    controller.push_error(ctx, GL_INVALID_OPERATION);
                    return;
                }

                need_reinstantiate = false;
                buffer->assign_handle(new_h);
            }
        }

        if (need_reinstantiate) {
            ctx->command_builder_->recreate_buffer(buffer->handle_value(), data, size, upload_hint);
        }

        buffer->assign_data_size(static_cast<std::uint32_t>(size));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_buffer_sub_data_emu, std::uint32_t target, std::int32_t offset, std::int32_t size, const void *data) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        gles1_driver_buffer *buffer = ctx->binded_buffer(target == GL_ARRAY_BUFFER_EMU);
        if (!buffer || !buffer->handle_value()) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        if ((size < 0) || (offset < 0) || (offset + size >= static_cast<std::int32_t>(buffer->data_size()))) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        std::uint32_t size_upload_casted = static_cast<std::uint32_t>(size);
        ctx->command_builder_->update_buffer_data(buffer->handle_value(), static_cast<std::size_t>(offset), 1,
            &data, &size_upload_casted);
    }

    static bool gl_enum_to_drivers_data_format(const std::uint32_t param, drivers::data_format &res) {
        switch (param) {
        case GL_BYTE_EMU:
            res = drivers::data_format::sbyte;
            break;

        case GL_SHORT_EMU:
            res = drivers::data_format::sword;
            break;

        case GL_FLOAT_EMU:
            res = drivers::data_format::sfloat;
            break;

        case GL_FIXED_EMU:
            res = drivers::data_format::fixed;
            break;

        default:
            return false;
        }

        return true;
    }

    static bool assign_vertex_attrib_gles1(gles1_vertex_attrib &attrib, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset, std::uint32_t buffer_obj) {
        drivers::data_format temp;
        if (!gl_enum_to_drivers_data_format(type, temp)) {
            return false;
        }

        attrib.data_type_ = type;
        attrib.offset_ = static_cast<std::uint32_t>(offset);
        attrib.size_ = size;
        attrib.stride_ = stride;
        attrib.buffer_obj_ = buffer_obj;

        return true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_color_pointer_emu, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((size != 4) || (stride < 0)) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (!assign_vertex_attrib_gles1(ctx->color_attrib_, size, type, stride, offset, ctx->binded_array_buffer_handle_)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->attrib_changed_ = true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_normal_pointer_emu, std::uint32_t type, std::int32_t stride, std::uint32_t offset) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (stride < 0) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (!assign_vertex_attrib_gles1(ctx->normal_attrib_, 3, type, stride, offset, ctx->binded_array_buffer_handle_)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->attrib_changed_ = true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_vertex_pointer_emu, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((stride < 0) || ((size != 2) && (size != 3) && (size != 4))) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (!assign_vertex_attrib_gles1(ctx->vertex_attrib_, size, type, stride, offset, ctx->binded_array_buffer_handle_)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->attrib_changed_ = true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_texcoord_pointer_emu, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((stride < 0) || ((size != 2) && (size != 3) && (size != 4))) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (!assign_vertex_attrib_gles1(ctx->texture_units_[ctx->active_client_texture_unit_].coord_attrib_, size, type, stride, offset, ctx->binded_array_buffer_handle_)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->attrib_changed_ = true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_material_f_emu, std::uint32_t target, std::uint32_t pname, const float pvalue) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_FRONT_AND_BACK_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (pname == GL_SHININESS_EMU) {
            if ((pvalue < 0.0f) || (pvalue > 128.0f)) {
                controller.push_error(ctx, GL_INVALID_VALUE);
                return;
            }

            ctx->material_shininess_ = pvalue;
        } else {
            controller.push_error(ctx, GL_INVALID_ENUM);
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_material_x_emu, std::uint32_t target, std::uint32_t pname, const gl_fixed pvalue) {
        gl_material_f_emu(sys, target, pname, FIXED_32_TO_FLOAT(pvalue));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_material_fv_emu, std::uint32_t target, std::uint32_t pname, const float *pvalue) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_FRONT_AND_BACK_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        float *dest_params = nullptr;
        float *dest_params_2 = nullptr;

        switch (pname) {
        case GL_AMBIENT_EMU:
            dest_params = ctx->material_ambient_;
            break;

        case GL_DIFFUSE_EMU:
            dest_params = ctx->material_diffuse_;
            break;

        case GL_SPECULAR_EMU:
            dest_params = ctx->material_specular_;
            break;

        case GL_EMISSION_EMU:
            dest_params = ctx->material_emission_;
            break;

        case GL_AMBIENT_AND_DIFFUSE_EMU:
            dest_params = ctx->material_ambient_;
            dest_params_2 = ctx->material_diffuse_;

            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (dest_params) {
            std::memcpy(dest_params, pvalue, 4 * sizeof(float));
        }

        if (dest_params_2) {
            std::memcpy(dest_params_2, pvalue, 4 * sizeof(float));
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_material_xv_emu, std::uint32_t target, std::uint32_t pname, const gl_fixed *pvalue) {
        float converted[4];
        converted[0] = FIXED_32_TO_FLOAT(pvalue[0]);
        converted[1] = FIXED_32_TO_FLOAT(pvalue[1]);
        converted[2] = FIXED_32_TO_FLOAT(pvalue[2]);
        converted[3] = FIXED_32_TO_FLOAT(pvalue[3]);

        gl_material_fv_emu(sys, target, pname, converted);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_fog_f_emu, std::uint32_t target, const float param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (target) {
        case GL_FOG_START_EMU:
            ctx->fog_start_ = param;
            break;

        case GL_FOG_END_EMU:
            ctx->fog_end_ = param;
            break;

        case GL_FOG_DENSITY_EMU:
            ctx->fog_density_ = param;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_fog_x_emu, std::uint32_t target, const gl_fixed param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (target) {
        case GL_FOG_MODE_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_FOG_MODE_MASK;
            if (param == GL_LINEAR_EMU) {
                ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_FOG_MODE_LINEAR;
            } else if (param == GL_EXP_EMU) {
                ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_FOG_MODE_EXP;
            } else if (param == GL_EXP2_EMU) {
                ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_FOG_MODE_EXP2;
            } else {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            break;

        case GL_FOG_START_EMU:
            ctx->fog_start_ = FIXED_32_TO_FLOAT(param);
            break;

        case GL_FOG_END_EMU:
            ctx->fog_end_ = FIXED_32_TO_FLOAT(param);
            break;

        case GL_FOG_DENSITY_EMU:
            ctx->fog_density_ = FIXED_32_TO_FLOAT(param);
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_fog_fv_emu, std::uint32_t target, const float *param) {
        if ((target == GL_FOG_START_EMU) || (target == GL_FOG_END_EMU) || (target == GL_FOG_DENSITY_EMU)) {
            gl_fog_f_emu(sys, target, *param);
            return;
        }
        
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target == GL_FOG_COLOR_EMU) {
            ctx->fog_color_[0] = param[0];
            ctx->fog_color_[1] = param[1];
            ctx->fog_color_[2] = param[2];
            ctx->fog_color_[3] = param[3];
        } else {
            controller.push_error(ctx, GL_INVALID_ENUM);
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_fog_xv_emu, std::uint32_t target, const gl_fixed *param) {
        if ((target == GL_FOG_MODE_EMU) || (target == GL_FOG_START_EMU) || (target == GL_FOG_END_EMU) || (target == GL_FOG_DENSITY_EMU)) {
            gl_fog_x_emu(sys, target, *param);
            return;
        }
        
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target == GL_FOG_COLOR_EMU) {
            ctx->fog_color_[0] = FIXED_32_TO_FLOAT(param[0]);
            ctx->fog_color_[1] = FIXED_32_TO_FLOAT(param[1]);
            ctx->fog_color_[2] = FIXED_32_TO_FLOAT(param[2]);
            ctx->fog_color_[3] = FIXED_32_TO_FLOAT(param[3]);
        } else {
            controller.push_error(ctx, GL_INVALID_ENUM);
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_color_mask_emu, std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->color_mask_ = 0;
        if (red) ctx->color_mask_ |= 1;
        if (green) ctx->color_mask_ |= 2;
        if (blue) ctx->color_mask_ |= 4;
        if (alpha) ctx->color_mask_ |= 8;

        ctx->command_builder_->set_color_mask(ctx->color_mask_);
    }

    
    BRIDGE_FUNC_LIBRARY(void, gl_blend_func_emu, std::uint32_t source_factor, std::uint32_t dest_factor) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        drivers::blend_factor source_factor_driver;
        drivers::blend_factor dest_factor_driver;

        if (!convert_gl_factor_to_driver_enum(source_factor, source_factor_driver) || !convert_gl_factor_to_driver_enum(dest_factor, dest_factor_driver)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->source_blend_factor_ = source_factor_driver;
        ctx->dest_blend_factor_ = dest_factor_driver;

        ctx->command_builder_->blend_formula(drivers::blend_equation::add, drivers::blend_equation::add, source_factor_driver, dest_factor_driver,
            source_factor_driver, dest_factor_driver);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_stencil_mask_emu, std::uint32_t mask) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->stencil_mask_ = mask;
        ctx->command_builder_->set_stencil_mask(drivers::rendering_face::back_and_front, mask);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_depth_mask_emu, std::uint32_t mask) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->depth_mask_ = mask;
        ctx->command_builder_->set_depth_mask(mask);
    }

    static bool convert_gl_texenv_operand_to_our_enum(std::uint32_t value, gles_texture_env_info::source_operand &op, const bool is_for_alpha) {
        switch (value) {
        case GL_SRC_ALPHA_EMU:
            op = gles_texture_env_info::SOURCE_OPERAND_ALPHA;
            break;

        case GL_ONE_MINUS_SRC_ALPHA_EMU:
            op = gles_texture_env_info::SOURCE_OPERAND_ONE_MINUS_ALPHA;
            break;

        case GL_SRC_COLOR_EMU:
            if (is_for_alpha) {
                return false;
            }

            op = gles_texture_env_info::SOURCE_OPERAND_COLOR;
            break;

        case GL_ONE_MINUS_SRC_COLOR_EMU:
            if (is_for_alpha) {
                return false;
            }

            op = gles_texture_env_info::SOURCE_OPERAND_ONE_MINUS_COLOR;
            break;

        default:
            return false;
        }

        return true;
    }

    static bool convert_gl_texenv_src_to_our_enum(std::uint32_t value, gles_texture_env_info::source_type &tt) {
        switch (value) {
        case GL_TEXTURE_EMU:
            tt = gles_texture_env_info::SOURCE_TYPE_CURRENT_TEXTURE;
            break;

        case GL_CONSTANT_EMU:
            tt = gles_texture_env_info::SOURCE_TYPE_CONSTANT;
            break;

        case GL_PRIMARY_COLOR_EMU:
            tt = gles_texture_env_info::SOURCE_TYPE_PRIM_COLOR;
            break;

        case GL_PREVIOUS_EMU:
            tt = gles_texture_env_info::SOURCE_TYPE_PREVIOUS;
            break;

        case GL_TEXTURE0_EMU:
            tt = gles_texture_env_info::SOURCE_TYPE_TEXTURE_STAGE_0;
            break;

        case GL_TEXTURE1_EMU:
            tt = gles_texture_env_info::SOURCE_TYPE_TEXTURE_STAGE_1;
            break;

        case GL_TEXTURE2_EMU:
            tt = gles_texture_env_info::SOURCE_TYPE_TEXTURE_STAGE_2;
            break;

        default:
            return false;
        }

        return true;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_tex_envi_emu, std::uint32_t target, std::uint32_t name, std::int32_t param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_TEX_ENV_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        gles_texture_unit &unit = ctx->texture_units_[ctx->active_texture_unit_];
        gles_texture_env_info &info = unit.env_info_;

        switch (name) {
        case GL_TEX_ENV_MODE_EMU:
            switch (param) {
            case GL_ADD_EMU:
                info.env_mode_ = gles_texture_env_info::ENV_MODE_ADD;
                break;

            case GL_BLEND_EMU:
                info.env_mode_ = gles_texture_env_info::ENV_MODE_BLEND;
                break;

            case GL_DECAL_EMU:
                info.env_mode_ = gles_texture_env_info::ENV_MODE_DECAL;
                break;

            case GL_MODULATE_EMU:
                info.env_mode_ = gles_texture_env_info::ENV_MODE_MODULATE;
                break;

            case GL_REPLACE_EMU:
                info.env_mode_ = gles_texture_env_info::ENV_MODE_REPLACE;
                break;

            case GL_COMBINE_EMU:
                info.env_mode_ = gles_texture_env_info::ENV_MODE_COMBINE;
                break;

            default:
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            break;

        case GL_COMBINE_RGB_EMU:
        case GL_COMBINE_ALPHA_EMU: {
            gles_texture_env_info::source_combine_function function;
            switch (param) {
            case GL_REPLACE_EMU:
                function = gles_texture_env_info::SOURCE_COMBINE_REPLACE;
                break;

            case GL_MODULATE_EMU:
                function = gles_texture_env_info::SOURCE_COMBINE_MODULATE;
                break;

            case GL_ADD_EMU:
                function = gles_texture_env_info::SOURCE_COMBINE_ADD;
                break;

            case GL_ADD_SIGNED_EMU:
                function = gles_texture_env_info::SOURCE_COMBINE_ADD_SIGNED;
                break;

            case GL_INTERPOLATE_EMU:
                function = gles_texture_env_info::SOURCE_COMBINE_INTERPOLATE;
                break;

            case GL_SUBTRACT_EMU:
                function = gles_texture_env_info::SOURCE_COMBINE_SUBTRACT;
                break;

            case GL_DOT3_RGB_EMU:
                function = gles_texture_env_info::SOURCE_COMBINE_DOT3_RGB;
                break;

            case GL_DOT3_RGBA_EMU:
                function = gles_texture_env_info::SOURCE_COMBINE_DOT3_RGBA;
                break;

            default:
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            if (name == GL_COMBINE_RGB_EMU) {
                info.combine_rgb_func_ = function;
            } else {
                info.combine_a_func_ = function;
            }

            break;
        }

        case GL_SRC0_ALPHA_EMU: {
            gles_texture_env_info::source_type type;
            if (!convert_gl_texenv_src_to_our_enum(param, type)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src0_a_ = type;
            break;
        }

        case GL_SRC0_RGB_EMU: {
            gles_texture_env_info::source_type type;
            if (!convert_gl_texenv_src_to_our_enum(param, type)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src0_rgb_ = type;
            break;
        }

        case GL_SRC1_ALPHA_EMU: {
            gles_texture_env_info::source_type type;
            if (!convert_gl_texenv_src_to_our_enum(param, type)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src1_a_ = type;
            break;
        }

        case GL_SRC1_RGB_EMU: {
            gles_texture_env_info::source_type type;
            if (!convert_gl_texenv_src_to_our_enum(param, type)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src1_rgb_ = type;
            break;
        }

        case GL_SRC2_ALPHA_EMU: {
            gles_texture_env_info::source_type type;
            if (!convert_gl_texenv_src_to_our_enum(param, type)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src2_a_ = type;
            break;
        }

        case GL_SRC2_RGB_EMU: {
            gles_texture_env_info::source_type type;
            if (!convert_gl_texenv_src_to_our_enum(param, type)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src2_rgb_ = type;
            break;
        }

        case GL_OPERAND0_RGB_EMU: {
            gles_texture_env_info::source_operand operand;
            if (!convert_gl_texenv_operand_to_our_enum(param, operand, false)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src0_rgb_op_ = operand;
            break;
        }

        case GL_OPERAND0_ALPHA_EMU: {
            gles_texture_env_info::source_operand operand;
            if (!convert_gl_texenv_operand_to_our_enum(param, operand, true)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src0_a_op_ = operand;
            break;
        }
        
        case GL_OPERAND1_RGB_EMU: {
            gles_texture_env_info::source_operand operand;
            if (!convert_gl_texenv_operand_to_our_enum(param, operand, false)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src1_rgb_op_ = operand;
            break;
        }

        case GL_OPERAND1_ALPHA_EMU: {
            gles_texture_env_info::source_operand operand;
            if (!convert_gl_texenv_operand_to_our_enum(param, operand, true)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src1_a_op_ = operand;
            break;
        }
        
        case GL_OPERAND2_RGB_EMU: {
            gles_texture_env_info::source_operand operand;
            if (!convert_gl_texenv_operand_to_our_enum(param, operand, false)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src2_rgb_op_ = operand;
            break;
        }

        case GL_OPERAND2_ALPHA_EMU: {
            gles_texture_env_info::source_operand operand;
            if (!convert_gl_texenv_operand_to_our_enum(param, operand, true)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src2_a_op_ = operand;
            break;
        }

        case GL_RGB_SCALE_EMU:
            if ((param != 1) && (param != 2) && (param != 4)) {
                controller.push_error(ctx, GL_INVALID_VALUE);
                return;
            }

            unit.rgb_scale_ = static_cast<std::uint8_t>(param);
            break;

        case GL_ALPHA_SCALE_EMU:
            if ((param != 1) && (param != 2) && (param != 4)) {
                controller.push_error(ctx, GL_INVALID_VALUE);
                return;
            }

            unit.alpha_scale_ = static_cast<std::uint8_t>(param);
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_tex_envf_emu, std::uint32_t target, std::uint32_t name, float param) {
        gl_tex_envi_emu(sys, target, name, static_cast<std::int32_t>(param));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_envx_emu, std::uint32_t target, std::uint32_t name, gl_fixed param) {
        std::int32_t param_casted = static_cast<std::int32_t>(param);
        
        if ((name == GL_ALPHA_SCALE_EMU) || (name == GL_RGB_SCALE_EMU)) {
            param_casted = static_cast<std::int32_t>(FIXED_32_TO_FLOAT(param)); 
        }

        gl_tex_envi_emu(sys, target, name, param_casted);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_tex_enviv_emu, std::uint32_t target, std::uint32_t name, const std::int32_t *param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target == GL_TEX_ENV_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (name == GL_TEX_ENV_COLOR_EMU) {
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[0] = param[0] / 255.0f;
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[1] = param[1] / 255.0f;
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[2] = param[2] / 255.0f;
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[3] = param[3] / 255.0f;

            return;
        }

        gl_tex_envi_emu(sys, target, name, *param);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_envfv_emu, std::uint32_t target, std::uint32_t name, const float *param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target == GL_TEX_ENV_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (name == GL_TEX_ENV_COLOR_EMU) {
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[0] = param[0];
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[1] = param[1];
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[2] = param[2];
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[3] = param[3];

            return;
        }

        gl_tex_envi_emu(sys, target, name, static_cast<std::int32_t>(*param));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_envxv_emu, std::uint32_t target, std::uint32_t name, const gl_fixed *param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target == GL_TEX_ENV_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (name == GL_TEX_ENV_COLOR_EMU) {
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[0] = FIXED_32_TO_FLOAT(param[0]);
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[1] = FIXED_32_TO_FLOAT(param[1]);
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[2] = FIXED_32_TO_FLOAT(param[2]);
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[3] = FIXED_32_TO_FLOAT(param[3]);

            return;
        }

        std::int32_t param_casted = static_cast<std::int32_t>(*param);
        
        if ((name == GL_ALPHA_SCALE_EMU) || (name == GL_RGB_SCALE_EMU)) {
            param_casted = static_cast<std::int32_t>(FIXED_32_TO_FLOAT(*param)); 
        }

        gl_tex_envi_emu(sys, target, name, param_casted);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_get_pointerv_emu, std::uint32_t target, std::uint32_t *off) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (target) {
        case GL_VERTEX_ARRAY_POINTER_EMU:
            *off = ctx->vertex_attrib_.offset_;
            break;

        case GL_COLOR_ARRAY_POINTER_EMU:
            *off = ctx->color_attrib_.offset_;
            break;

        case GL_TEXTURE_COORD_ARRAY_POINTER_EMU:
            *off = ctx->texture_units_[ctx->active_texture_unit_].coord_attrib_.offset_;
            break;

        case GL_NORMAL_ARRAY_POINTER_EMU:
            *off = ctx->normal_attrib_.offset_;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_light_f_emu, std::uint32_t light, std::uint32_t pname, float param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((light < GL_LIGHT0_EMU) || (light >= GL_LIGHT0_EMU + GLES1_EMU_MAX_LIGHT)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        const std::uint32_t light_index = light - GL_LIGHT0_EMU;
        gles1_light_info &info = ctx->lights_[light_index];

        switch (pname) {
        case GL_SPOT_EXPONENT_EMU:
            info.spot_exponent_ = param;
            break;

        case GL_SPOT_CUTOFF_EMU:
            info.spot_cutoff_ = param;
            break;

        case GL_CONSTANT_ATTENUATION_EMU:
            info.attenuatation_[0] = param;
            break;

        case GL_LINEAR_ATTENUATION_EMU:
            info.attenuatation_[1] = param;
            break;

        case GL_QUADRATIC_ATTENUATION_EMU:
            info.attenuatation_[2] = param;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_light_x_emu, std::uint32_t light, std::uint32_t pname, gl_fixed param) {
        gl_light_f_emu(sys, light, pname, FIXED_32_TO_FLOAT(param));
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_light_fv_emu, std::uint32_t light, std::uint32_t pname, const float *param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((light < GL_LIGHT0_EMU) || (light >= GL_LIGHT0_EMU + GLES1_EMU_MAX_LIGHT)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        const std::uint32_t light_index = light - GL_LIGHT0_EMU;
        gles1_light_info &info = ctx->lights_[light_index];

        switch (pname) {
        case GL_SPOT_EXPONENT_EMU:
            info.spot_exponent_ = *param;
            break;

        case GL_SPOT_CUTOFF_EMU:
            info.spot_cutoff_ = *param;
            break;

        case GL_CONSTANT_ATTENUATION_EMU:
            info.attenuatation_[0] = *param;
            break;

        case GL_LINEAR_ATTENUATION_EMU:
            info.attenuatation_[1] = *param;
            break;

        case GL_QUADRATIC_ATTENUATION_EMU:
            info.attenuatation_[2] = *param;
            break;

        case GL_AMBIENT_EMU:
            std::memcpy(info.ambient_, param, 4 * sizeof(float));
            break;

        case GL_DIFFUSE_EMU:
            std::memcpy(info.diffuse_, param, 4 * sizeof(float));
            break;

        case GL_SPECULAR_EMU:
            std::memcpy(info.specular_, param, 4 * sizeof(float));
            break;

        case GL_POSITION_EMU: {
            std::memcpy(info.position_or_dir_, param, 4 * sizeof(float));
            glm::vec4 transformed_pos = ctx->model_view_mat_stack_.top() * glm::make_vec4(info.position_or_dir_);

            info.position_or_dir_transformed_[0] = transformed_pos.x;
            info.position_or_dir_transformed_[1] = transformed_pos.y;
            info.position_or_dir_transformed_[2] = transformed_pos.z;
            info.position_or_dir_transformed_[3] = info.position_or_dir_[3];
            
            break;
        }

        case GL_SPOT_DIRECTION_EMU: {
            std::memcpy(info.spot_dir_, param, 3 * sizeof(float));

            glm::vec3 transformed_spot_dir = (glm::mat3(ctx->model_view_mat_stack_.top()) * glm::make_vec3(info.spot_dir_)) * glm::vec3(-1);

            info.spot_dir_transformed_[0] = transformed_spot_dir.x;
            info.spot_dir_transformed_[1] = transformed_spot_dir.y;
            info.spot_dir_transformed_[2] = transformed_spot_dir.z;
            
            break;
        }

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_light_xv_emu, std::uint32_t light, std::uint32_t pname, const gl_fixed *param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((light < GL_LIGHT0_EMU) || (light >= GL_LIGHT0_EMU + GLES1_EMU_MAX_LIGHT)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        const std::uint32_t light_index = light - GL_LIGHT0_EMU;
        gles1_light_info &info = ctx->lights_[light_index];

        switch (pname) {
        case GL_SPOT_EXPONENT_EMU:
            info.spot_exponent_ = FIXED_32_TO_FLOAT(*param);
            break;

        case GL_SPOT_CUTOFF_EMU:
            info.spot_cutoff_ = FIXED_32_TO_FLOAT(*param);
            break;

        case GL_CONSTANT_ATTENUATION_EMU:
            info.attenuatation_[0] = FIXED_32_TO_FLOAT(*param);
            break;

        case GL_LINEAR_ATTENUATION_EMU:
            info.attenuatation_[1] = FIXED_32_TO_FLOAT(*param);
            break;

        case GL_QUADRATIC_ATTENUATION_EMU:
            info.attenuatation_[2] = FIXED_32_TO_FLOAT(*param);
            break;

        case GL_AMBIENT_EMU:
            info.ambient_[0] = FIXED_32_TO_FLOAT(param[0]);
            info.ambient_[1] = FIXED_32_TO_FLOAT(param[1]);
            info.ambient_[2] = FIXED_32_TO_FLOAT(param[2]);
            info.ambient_[3] = FIXED_32_TO_FLOAT(param[3]);
            break;

        case GL_DIFFUSE_EMU:
            info.diffuse_[0] = FIXED_32_TO_FLOAT(param[0]);
            info.diffuse_[1] = FIXED_32_TO_FLOAT(param[1]);
            info.diffuse_[2] = FIXED_32_TO_FLOAT(param[2]);
            info.diffuse_[3] = FIXED_32_TO_FLOAT(param[3]);
            break;

        case GL_SPECULAR_EMU:
            info.specular_[0] = FIXED_32_TO_FLOAT(param[0]);
            info.specular_[1] = FIXED_32_TO_FLOAT(param[1]);
            info.specular_[2] = FIXED_32_TO_FLOAT(param[2]);
            info.specular_[3] = FIXED_32_TO_FLOAT(param[3]);
            break;

        case GL_POSITION_EMU: {
            info.position_or_dir_[0] = FIXED_32_TO_FLOAT(param[0]);
            info.position_or_dir_[1] = FIXED_32_TO_FLOAT(param[1]);
            info.position_or_dir_[2] = FIXED_32_TO_FLOAT(param[2]);
            info.position_or_dir_[3] = FIXED_32_TO_FLOAT(param[3]);
            
            glm::vec4 transformed_pos = ctx->model_view_mat_stack_.top() * glm::make_vec4(info.position_or_dir_);

            info.position_or_dir_transformed_[0] = transformed_pos.x;
            info.position_or_dir_transformed_[1] = transformed_pos.y;
            info.position_or_dir_transformed_[2] = transformed_pos.z;
            info.position_or_dir_transformed_[3] = info.position_or_dir_[3];
            
            break;
        }

        case GL_SPOT_DIRECTION_EMU: {
            info.spot_dir_[0] = FIXED_32_TO_FLOAT(param[0]);
            info.spot_dir_[1] = FIXED_32_TO_FLOAT(param[1]);
            info.spot_dir_[2] = FIXED_32_TO_FLOAT(param[2]);
            
            glm::vec3 transformed_spot_dir = (glm::mat3(ctx->model_view_mat_stack_.top()) * glm::make_vec3(info.spot_dir_)) * glm::vec3(-1);

            info.spot_dir_transformed_[0] = transformed_spot_dir.x;
            info.spot_dir_transformed_[1] = transformed_spot_dir.y;
            info.spot_dir_transformed_[2] = transformed_spot_dir.z;
            
            break;
        }

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_light_model_f_emu, std::uint32_t pname, float param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (pname != GL_LIGHT_MODEL_TWO_SIDE_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (param != 0.0f) {
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT_TWO_SIDE;
        } else {
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT_TWO_SIDE;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_light_model_x_emu, std::uint32_t pname, gl_fixed param) {
        gl_light_model_f_emu(sys, pname, FIXED_32_TO_FLOAT(param));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_light_model_fv_emu, std::uint32_t pname, float *param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (pname) {
        case GL_LIGHT_MODEL_AMBIENT_EMU:
            ctx->global_ambient_[0] = param[0];
            ctx->global_ambient_[1] = param[1];
            ctx->global_ambient_[2] = param[2];
            ctx->global_ambient_[3] = param[3];
            break;

        case GL_LIGHT_MODEL_TWO_SIDE_EMU:
            if (*param != 0.0f) {
                ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT_TWO_SIDE;
            } else {
                ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT_TWO_SIDE;
            }
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_light_model_xv_emu, std::uint32_t pname, gl_fixed *param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (pname) {
        case GL_LIGHT_MODEL_AMBIENT_EMU:
            ctx->global_ambient_[0] = FIXED_32_TO_FLOAT(param[0]);
            ctx->global_ambient_[1] = FIXED_32_TO_FLOAT(param[1]);
            ctx->global_ambient_[2] = FIXED_32_TO_FLOAT(param[2]);
            ctx->global_ambient_[3] = FIXED_32_TO_FLOAT(param[3]);
            break;

        case GL_LIGHT_MODEL_TWO_SIDE_EMU:
            if (*param != 0) {
                ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT_TWO_SIDE;
            } else {
                ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT_TWO_SIDE;
            }
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_depth_func_emu, std::uint32_t func) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        drivers::condition_func func_drv = drivers::condition_func::always;
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!cond_func_from_gl_enum(func, func_drv)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->depth_func_ = func;
        ctx->command_builder_->set_depth_pass_condition(func_drv);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_enable_emu, std::uint32_t cap) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (cap) {
        case GL_ALPHA_TEST_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_ALPHA_TEST;
            break;

        case GL_BLEND_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_BLEND_ENABLE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::blend, true);

            break;

        case GL_COLOR_LOGIC_OP_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_COLOR_LOGIC_OP_ENABLE;
            break;

        case GL_CLIP_PLANE0_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE0_ENABLE;
            break;
            
        case GL_CLIP_PLANE1_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE1_ENABLE;
            break;

        case GL_CLIP_PLANE2_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE2_ENABLE;
            break;

        case GL_CLIP_PLANE3_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE3_ENABLE;
            break;

        case GL_CLIP_PLANE4_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE4_ENABLE;
            break;

        case GL_CLIP_PLANE5_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE5_ENABLE;
            break;

        case GL_CULL_FACE_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_CULL_FACE_ENABLE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::cull, true);

            break;

        case GL_COLOR_MATERIAL_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_COLOR_MATERIAL_ENABLE;
            break;

        case GL_DEPTH_TEST_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_DEPTH_TEST_ENABLE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::depth_test, true);

            break;

        case GL_STENCIL_TEST_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_STENCIL_TEST_ENABLE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::stencil_test, true);

            break;

        case GL_FOG_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_FOG_ENABLE;
            break;

        case GL_LIGHTING_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHTING_ENABLE;
            break;

        case GL_LIGHT0_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT0_ON;
            break;

        case GL_LIGHT1_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT1_ON;
            break;

        case GL_LIGHT2_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT2_ON;
            break;

        case GL_LIGHT3_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT3_ON;
            break;

        case GL_LIGHT4_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT4_ON;
            break;

        case GL_LIGHT5_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT5_ON;
            break;

        case GL_LIGHT6_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT6_ON;
            break;
        
        case GL_LIGHT7_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT7_ON;
            break;

        case GL_LINE_SMOOTH_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_LINE_SMOOTH;
            ctx->command_builder_->set_feature(drivers::graphics_feature::line_smooth, true);

            break;

        case GL_DITHER_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_DITHER;
            ctx->command_builder_->set_feature(drivers::graphics_feature::dither, true);

            break;

        case GL_SCISSOR_TEST_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_SCISSOR_ENABLE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::clipping, true);

            break;

        case GL_SAMPLE_COVERAGE_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_SAMPLE_COVERAGE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::sample_coverage, true);

            break;

        case GL_MULTISAMPLE_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_MULTISAMPLE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::multisample, true);

            break;

        case GL_SAMPLE_ALPHA_TO_COVERAGE_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_SAMPLE_ALPHA_TO_COVERAGE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::sample_alpha_to_coverage, true);

            break;

        case GL_SAMPLE_ALPHA_TO_ONE_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_SAMPLE_ALPHA_TO_ONE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::sample_alpha_to_one, true);

        case GL_TEXTURE_2D_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_TEXTURE_ENABLE;
            break;

        case GL_NORMALIZE_EMU:
            ctx->vertex_statuses_ |= egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_NORMALIZE;
            break;

        case GL_RESCALE_NORMAL_EMU:
            ctx->vertex_statuses_ |= egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_RESCALE;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_disable_emu, std::uint32_t cap) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (cap) {
        case GL_ALPHA_TEST_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_ALPHA_TEST;
            break;

        case GL_BLEND_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_BLEND_ENABLE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::blend, false);

            break;

        case GL_COLOR_LOGIC_OP_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_COLOR_LOGIC_OP_ENABLE;
            break;

        case GL_CLIP_PLANE0_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE0_ENABLE;
            break;
            
        case GL_CLIP_PLANE1_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE1_ENABLE;
            break;

        case GL_CLIP_PLANE2_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE2_ENABLE;
            break;

        case GL_CLIP_PLANE3_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE3_ENABLE;
            break;

        case GL_CLIP_PLANE4_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE4_ENABLE;
            break;

        case GL_CLIP_PLANE5_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE5_ENABLE;
            break;

        case GL_CULL_FACE_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_CULL_FACE_ENABLE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::cull, false);

            break;

        case GL_COLOR_MATERIAL_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_COLOR_MATERIAL_ENABLE;
            break;

        case GL_DEPTH_TEST_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_DEPTH_TEST_ENABLE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::depth_test, false);

            break;

        case GL_STENCIL_TEST_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_STENCIL_TEST_ENABLE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::stencil_test, false);

            break;

        case GL_FOG_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_FOG_ENABLE;
            break;

        case GL_LIGHTING_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHTING_ENABLE;
            break;

        case GL_LIGHT0_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT0_ON;
            break;

        case GL_LIGHT1_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT1_ON;
            break;

        case GL_LIGHT2_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT2_ON;
            break;

        case GL_LIGHT3_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT3_ON;
            break;

        case GL_LIGHT4_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT4_ON;
            break;

        case GL_LIGHT5_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT5_ON;
            break;

        case GL_LIGHT6_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT6_ON;
            break;
        
        case GL_LIGHT7_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT7_ON;
            break;

        case GL_LINE_SMOOTH_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_LINE_SMOOTH;
            ctx->command_builder_->set_feature(drivers::graphics_feature::line_smooth, false);

            break;

        case GL_DITHER_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_DITHER;
            ctx->command_builder_->set_feature(drivers::graphics_feature::dither, false);

            break;

        case GL_SCISSOR_TEST_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_SCISSOR_ENABLE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::clipping, false);

            break;

        case GL_SAMPLE_COVERAGE_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_SAMPLE_COVERAGE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::sample_coverage, false);

            break;

        case GL_MULTISAMPLE_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_MULTISAMPLE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::multisample, false);

            break;

        case GL_SAMPLE_ALPHA_TO_COVERAGE_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_SAMPLE_ALPHA_TO_COVERAGE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::sample_alpha_to_coverage, false);

            break;

        case GL_SAMPLE_ALPHA_TO_ONE_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_SAMPLE_ALPHA_TO_ONE;
            ctx->command_builder_->set_feature(drivers::graphics_feature::sample_alpha_to_one, false);
            break;

        case GL_TEXTURE_2D_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_TEXTURE_ENABLE;
            break;

        case GL_NORMALIZE_EMU:
            ctx->vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_NORMALIZE;
            break;

        case GL_RESCALE_NORMAL_EMU:
            ctx->vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_RESCALE;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_clip_plane_f_emu, std::uint32_t plane, float *eq) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((plane < GL_CLIP_PLANE0_EMU) && (plane > GL_CLIP_PLANE5_EMU)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        std::memcpy(ctx->clip_planes_[plane - GL_CLIP_PLANE0_EMU], eq, 4 * sizeof(float));

        glm::vec4 transformed = glm::inverse(glm::transpose(ctx->model_view_mat_stack_.top())) * glm::make_vec4(ctx->clip_planes_[plane - GL_CLIP_PLANE0_EMU]);
        ctx->clip_planes_transformed_[plane - GL_CLIP_PLANE0_EMU][0] = transformed.x;
        ctx->clip_planes_transformed_[plane - GL_CLIP_PLANE0_EMU][1] = transformed.y;
        ctx->clip_planes_transformed_[plane - GL_CLIP_PLANE0_EMU][2] = transformed.z;
        ctx->clip_planes_transformed_[plane - GL_CLIP_PLANE0_EMU][3] = transformed.w;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_clip_plane_x_emu, std::uint32_t plane, gl_fixed *eq) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((plane < GL_CLIP_PLANE0_EMU) && (plane > GL_CLIP_PLANE5_EMU)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        const std::uint32_t plane_index = plane - GL_CLIP_PLANE0_EMU;

        ctx->clip_planes_[plane_index][0] = FIXED_32_TO_FLOAT(eq[0]);
        ctx->clip_planes_[plane_index][1] = FIXED_32_TO_FLOAT(eq[1]);
        ctx->clip_planes_[plane_index][2] = FIXED_32_TO_FLOAT(eq[2]);
        ctx->clip_planes_[plane_index][3] = FIXED_32_TO_FLOAT(eq[3]); 

        glm::vec4 transformed = glm::make_vec4(ctx->clip_planes_[plane - GL_CLIP_PLANE0_EMU]) * glm::inverse(ctx->model_view_mat_stack_.top());
        ctx->clip_planes_transformed_[plane - GL_CLIP_PLANE0_EMU][0] = transformed.x;
        ctx->clip_planes_transformed_[plane - GL_CLIP_PLANE0_EMU][1] = transformed.y;
        ctx->clip_planes_transformed_[plane - GL_CLIP_PLANE0_EMU][2] = transformed.z;
        ctx->clip_planes_transformed_[plane - GL_CLIP_PLANE0_EMU][3] = transformed.w;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_viewport_emu, std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->viewport_bl_.top = eka2l1::vec2(x, y);
        ctx->viewport_bl_.size = eka2l1::vec2(width, height);

        ctx->command_builder_->set_viewport(eka2l1::rect(eka2l1::vec2(x, ctx->draw_surface_->dimension_.y - (y + height)),
            eka2l1::vec2(width, height)));
    }

    static bool gl_enum_to_addressing_option(const std::uint32_t param, drivers::addressing_option &res) {
        switch (param) {
        case GL_REPEAT_EMU:
            res = drivers::addressing_option::repeat;

        case GL_CLAMP_TO_EDGE_EMU:
            res = drivers::addressing_option::clamp_to_edge;

        default:
            return false;
        }

        return true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_i_emu, std::uint32_t target, std::uint32_t pname, std::int32_t param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        gles1_driver_texture *tex = ctx->binded_texture();
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!tex) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        if (target != GL_TEXTURE_2D_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        switch (pname) {
        case GL_TEXTURE_MAG_FILTER_EMU: {
            drivers::filter_option opt;
            switch (param) {
            case GL_LINEAR_EMU:
                opt = drivers::filter_option::linear;
                break;

            case GL_NEAREST_EMU:
                opt = drivers::filter_option::nearest;
                break;

            default:
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            tex->set_mag_filter(param);
            ctx->command_builder_->set_texture_filter(tex->handle_value(), false, opt);

            break;
        }

        case GL_TEXTURE_MIN_FILTER_EMU: {
            drivers::filter_option opt;
            switch (param) {
            case GL_LINEAR_EMU:
                opt = drivers::filter_option::linear;
                break;

            case GL_NEAREST_EMU:
                opt = drivers::filter_option::nearest;
                break;

            case GL_LINEAR_MIPMAP_LINEAR_EMU:
                opt = drivers::filter_option::linear_mipmap_linear;
                break;

            case GL_LINEAR_MIPMAP_NEAREST_EMU:
                opt = drivers::filter_option::linear_mipmap_nearest;
                break;

            case GL_NEAREST_MIPMAP_NEAREST_EMU:
                opt = drivers::filter_option::nearest_mipmap_nearest;
                break;

            case GL_NEAREST_MIPMAP_LINEAR_EMU:
                opt = drivers::filter_option::nearest_mipmap_linear;
                break;

            default:
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            tex->set_min_filter(param);
            ctx->command_builder_->set_texture_filter(tex->handle_value(), true, opt);

            break;
        }

        case GL_TEXTURE_WRAP_S_EMU: {
            drivers::addressing_option opt;
            if (!gl_enum_to_addressing_option(param, opt)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            tex->set_wrap_s(param);
            ctx->command_builder_->set_texture_addressing_mode(tex->handle_value(), drivers::addressing_direction::s,
                opt);

            break;
        }
        
        case GL_TEXTURE_WRAP_T_EMU: {
            drivers::addressing_option opt;
            if (!gl_enum_to_addressing_option(param, opt)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            tex->set_wrap_t(param);
            ctx->command_builder_->set_texture_addressing_mode(tex->handle_value(), drivers::addressing_direction::t,
                opt);

            break;
        }

        case GL_GENERATE_MIPMAP_EMU:
            tex->set_auto_regenerate_mipmap(param != 0);
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_f_emu, std::uint32_t target, std::uint32_t pname, float param) {
        gl_tex_parameter_i_emu(sys, target, pname, static_cast<std::int32_t>(param));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_x_emu, std::uint32_t target, std::uint32_t pname, gl_fixed param) {
        gl_tex_parameter_i_emu(sys, target, pname, static_cast<std::int32_t>(param));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_iv_emu, std::uint32_t target, std::uint32_t pname, std::int32_t* param) {
        gl_tex_parameter_i_emu(sys, target, pname, *param);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_fv_emu, std::uint32_t target, std::uint32_t pname, float* param) {
        gl_tex_parameter_i_emu(sys, target, pname, static_cast<std::int32_t>(*param));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_xv_emu, std::uint32_t target, std::uint32_t pname, gl_fixed* param) {
        gl_tex_parameter_i_emu(sys, target, pname, static_cast<std::int32_t>(*param));
    }

    static bool prepare_shader_program_for_draw(dispatch::egl_controller &controller, egl_context_es1 *ctx, const std::uint32_t active_texs) {
        dispatch::gles_texture_env_info info_arr[GLES1_EMU_MAX_TEXTURE_COUNT];
        for (std::int32_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
            info_arr[i] = ctx->texture_units_[i].env_info_;
        }

        dispatch::gles1_shader_variables_info *var_info = nullptr;

        drivers::handle program = controller.get_es1_shaderman().retrieve_program(ctx->vertex_statuses_, ctx->fragment_statuses_, active_texs,
            info_arr, var_info);

        if (!program) {
            LOG_ERROR(HLE_DISPATCHER, "Problem while retrieveing GLES1 shader program!");
            return false;
        }

        ctx->command_builder_->use_program(program);

        if (var_info) {
            // Not binded by uniform buffer/constant buffer
            ctx->command_builder_->set_dynamic_uniform(var_info->view_model_mat_loc_, drivers::shader_set_var_type::mat4,
                glm::value_ptr(ctx->model_view_mat_stack_.top()), 64);

            ctx->command_builder_->set_dynamic_uniform(var_info->proj_mat_loc_, drivers::shader_set_var_type::mat4,
                glm::value_ptr(ctx->proj_mat_stack_.top()), 64);

            if ((ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_COLOR_ARRAY) == 0) {
                ctx->command_builder_->set_dynamic_uniform(var_info->color_loc_, drivers::shader_set_var_type::vec4,
                    ctx->color_uniforms_, 16);
            }

            if ((ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY) == 0) {
                ctx->command_builder_->set_dynamic_uniform(var_info->normal_loc_, drivers::shader_set_var_type::vec3,
                    ctx->normal_uniforms_, 12);
            }

            for (std::uint32_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
                if (active_texs & (1 << i)) {
                    if ((ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD_ARRAY) == 0)
                        ctx->command_builder_->set_dynamic_uniform(var_info->texcoord_loc_[i], drivers::shader_set_var_type::vec4,
                            ctx->texture_units_[i].coord_uniforms_, 16);

                    ctx->command_builder_->set_dynamic_uniform(var_info->texture_mat_loc_[i], drivers::shader_set_var_type::mat4,
                        glm::value_ptr(ctx->texture_units_[i].texture_mat_stack_.top()), 64);

                    ctx->command_builder_->set_texture_for_shader(i, var_info->texview_loc_[i], drivers::shader_module_type::fragment);
                    ctx->command_builder_->set_dynamic_uniform(var_info->texenv_color_loc_[i], drivers::shader_set_var_type::vec4,
                        ctx->texture_units_[i].env_colors_, 16);
                }
            }
            
            for (std::uint8_t i = 0; i < GLES1_EMU_MAX_CLIP_PLANE; i++) {
                if (ctx->fragment_statuses_ & (1 << (egl_context_es1::FRAGMENT_STATE_CLIP_PLANE_BIT_POS + i))) {
                    ctx->command_builder_->set_dynamic_uniform(var_info->clip_plane_loc_[i], drivers::shader_set_var_type::vec4,
                        ctx->clip_planes_transformed_[i], 16);
                }
            }

            ctx->command_builder_->set_dynamic_uniform(var_info->material_ambient_loc_, drivers::shader_set_var_type::vec4,
                ctx->material_ambient_, 16);

            ctx->command_builder_->set_dynamic_uniform(var_info->material_diffuse_loc_, drivers::shader_set_var_type::vec4,
                ctx->material_diffuse_, 16);
            
            ctx->command_builder_->set_dynamic_uniform(var_info->material_specular_loc_, drivers::shader_set_var_type::vec4,
                ctx->material_specular_, 16);

            ctx->command_builder_->set_dynamic_uniform(var_info->material_emission_loc_, drivers::shader_set_var_type::vec4,
                ctx->material_emission_, 16);

            ctx->command_builder_->set_dynamic_uniform(var_info->material_shininess_loc_, drivers::shader_set_var_type::real,
                &ctx->material_shininess_, 4);

            ctx->command_builder_->set_dynamic_uniform(var_info->global_ambient_loc_, drivers::shader_set_var_type::vec4,
                ctx->global_ambient_, 16);

            if (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_ALPHA_TEST)
                ctx->command_builder_->set_dynamic_uniform(var_info->alpha_test_ref_loc_, drivers::shader_set_var_type::real,
                    &ctx->alpha_test_ref_, 4);

            if (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_FOG_ENABLE) {
                ctx->command_builder_->set_dynamic_uniform(var_info->fog_color_loc_, drivers::shader_set_var_type::vec4,
                    ctx->fog_color_, 16);

                if (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_FOG_MODE_LINEAR) {
                    ctx->command_builder_->set_dynamic_uniform(var_info->fog_start_loc_, drivers::shader_set_var_type::real,
                        &ctx->fog_start_, 4);
                    ctx->command_builder_->set_dynamic_uniform(var_info->fog_end_loc_, drivers::shader_set_var_type::real,
                        &ctx->fog_end_, 4);
                } else {
                    ctx->command_builder_->set_dynamic_uniform(var_info->fog_density_loc_, drivers::shader_set_var_type::real,
                        &ctx->fog_density_, 4);
                }
            }

            if (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHTING_ENABLE) {
                for (std::uint32_t i = 0, mask = egl_context_es1::FRAGMENT_STATE_LIGHT0_ON; i < GLES1_EMU_MAX_LIGHT; i++, mask <<= 1) {
                    if (ctx->fragment_statuses_ & mask) {
                        ctx->command_builder_->set_dynamic_uniform(var_info->light_dir_or_pos_loc_[i], drivers::shader_set_var_type::vec4,
                            ctx->lights_[i].position_or_dir_transformed_, 16);
                        ctx->command_builder_->set_dynamic_uniform(var_info->light_ambient_loc_[i], drivers::shader_set_var_type::vec4,
                            ctx->lights_[i].ambient_, 16);
                        ctx->command_builder_->set_dynamic_uniform(var_info->light_diffuse_loc_[i], drivers::shader_set_var_type::vec4,
                            ctx->lights_[i].diffuse_, 16);
                        ctx->command_builder_->set_dynamic_uniform(var_info->light_specular_loc_[i], drivers::shader_set_var_type::vec4,
                            ctx->lights_[i].specular_, 16);
                        ctx->command_builder_->set_dynamic_uniform(var_info->light_spot_dir_loc_[i], drivers::shader_set_var_type::vec3,
                            ctx->lights_[i].spot_dir_transformed_, 12);
                        ctx->command_builder_->set_dynamic_uniform(var_info->light_spot_cutoff_loc_[i], drivers::shader_set_var_type::real,
                            &ctx->lights_[i].spot_cutoff_, 4);
                        ctx->command_builder_->set_dynamic_uniform(var_info->light_spot_exponent_loc_[i], drivers::shader_set_var_type::real,
                            &ctx->lights_[i].spot_exponent_, 4);
                        ctx->command_builder_->set_dynamic_uniform(var_info->light_attenuatation_vec_loc_[i], drivers::shader_set_var_type::vec3,
                            ctx->lights_[i].attenuatation_, 12);
                    }
                }
            }
        } else {
            LOG_WARN(HLE_DISPATCHER, "Shader variables in buffer not yet supported!");
        }

        return true;
    }

    static std::uint32_t retrieve_buffer_size_attrib_es1(const std::uint32_t type, const std::uint32_t comp_count, const std::uint32_t estvcount) {
        std::uint32_t bytes_per_comp = 0;
        switch (type) {
        case GL_BYTE_EMU:
            bytes_per_comp = 1;
            break;

        case GL_SHORT_EMU:
            bytes_per_comp = 2;
            break;

        case GL_FLOAT_EMU:
            bytes_per_comp = 4;
            break;

        case GL_FIXED_EMU:
            bytes_per_comp = 4;
            break;

        default:
            return 0;
        }

        return bytes_per_comp * comp_count * estvcount;
    }

    static void prepare_vertex_buffer_and_descriptors(egl_context_es1 *ctx, drivers::graphics_driver *drv, kernel::process *crr_process, const std::uint32_t vcount, const std::uint32_t active_texs) {
        auto update_cpu_buffer_data = [&](gles1_vertex_attrib &attrib) -> bool {
            if (attrib.buffer_obj_ != 0) {
                return true;
            }

            std::uint8_t *data = eka2l1::ptr<std::uint8_t>(attrib.offset_).get(crr_process);
            if (!data) {
                LOG_WARN(HLE_DISPATCHER, "Invalid pointer to CPU buffer data for attribute!");
                return false;
            }

            // For elements draw this will doom (we don't know how many vertices to be exact at that situation)
            const std::uint32_t est_buf_size = retrieve_buffer_size_attrib_es1(attrib.data_type_, attrib.size_, vcount);

            if (!attrib.in_house_buffer_) {
                attrib.in_house_buffer_ = drivers::create_buffer(drv, data, est_buf_size, static_cast<drivers::buffer_upload_hint>(drivers::buffer_upload_dynamic | drivers::buffer_upload_draw));
            } else {
                std::uint32_t size_casted = static_cast<std::uint32_t>(est_buf_size);
                const void *data_casted = data;

                ctx->command_builder_->update_buffer_data(attrib.in_house_buffer_, 0, 1, &data_casted, &size_casted);
            }

            return true;
        };

        if (!update_cpu_buffer_data(ctx->vertex_attrib_)) {
            return;
        }

        if (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_COLOR_ARRAY) {
            update_cpu_buffer_data(ctx->color_attrib_);
        }

        if (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY) {
            update_cpu_buffer_data(ctx->normal_attrib_);
        }

        if (active_texs && (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD_ARRAY)) {
            for (std::int32_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
                if (active_texs & (1 << i)) {
                    update_cpu_buffer_data(ctx->texture_units_[i].coord_attrib_);
                }
            }
        }

        if (ctx->attrib_changed_) {
            std::vector<drivers::handle> vertex_buffers_alloc;

            auto retrieve_vertex_buffer_slot = [&](gles1_vertex_attrib attrib, std::uint32_t &res) -> bool {
                drivers::handle buffer_handle_drv = 0;
                if (attrib.buffer_obj_ == 0) {
                    buffer_handle_drv = attrib.in_house_buffer_;
                } else {
                    auto *buffer_inst_ptr = ctx->objects_.get(attrib.buffer_obj_);
                    if (!buffer_inst_ptr || ((*buffer_inst_ptr)->object_type() != GLES1_OBJECT_BUFFER)) {
                        return false;
                    }

                    gles1_driver_buffer *buffer = reinterpret_cast<gles1_driver_buffer*>((*buffer_inst_ptr).get());
                    buffer_handle_drv = buffer->handle_value();
                }

                auto ite = std::find(vertex_buffers_alloc.begin(), vertex_buffers_alloc.end(), buffer_handle_drv);
                if (ite != vertex_buffers_alloc.end()) {
                    res = static_cast<std::uint32_t>(std::distance(vertex_buffers_alloc.begin(), ite));
                    return true;
                }

                vertex_buffers_alloc.push_back(buffer_handle_drv);
                res = static_cast<std::uint32_t>(vertex_buffers_alloc.size() - 1);

                return true;
            };

            // Remade and attach descriptors
            std::vector<drivers::input_descriptor> descs;
            drivers::data_format temp_format;

            drivers::input_descriptor temp_desc;
            if (!retrieve_vertex_buffer_slot(ctx->vertex_attrib_, temp_desc.buffer_slot)) {
                LOG_WARN(HLE_DISPATCHER, "Vertex attribute not bound to a valid buffer, draw call skipping!");
                return;
            }

            temp_desc.location = 0;

            if (ctx->vertex_attrib_.buffer_obj_ != 0)
                temp_desc.offset = ctx->vertex_attrib_.offset_;
            else
                temp_desc.offset = 0;

            temp_desc.stride = ctx->vertex_attrib_.stride_;

            gl_enum_to_drivers_data_format(ctx->vertex_attrib_.data_type_, temp_format);
            temp_desc.set_format(ctx->vertex_attrib_.size_, temp_format);

            descs.push_back(temp_desc);

            if (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_COLOR_ARRAY) {
                if (retrieve_vertex_buffer_slot(ctx->color_attrib_, temp_desc.buffer_slot)) {
                    temp_desc.location = 1;
                    if (ctx->color_attrib_.buffer_obj_ != 0)
                        temp_desc.offset = ctx->color_attrib_.offset_;
                    else
                        temp_desc.offset = 0;

                    temp_desc.stride = ctx->color_attrib_.stride_;

                    gl_enum_to_drivers_data_format(ctx->color_attrib_.data_type_, temp_format);
                    temp_desc.set_format(ctx->color_attrib_.size_, temp_format);

                    descs.push_back(temp_desc);
                }
            }

            if (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY) {
                if (retrieve_vertex_buffer_slot(ctx->normal_attrib_, temp_desc.buffer_slot)) {
                    temp_desc.location = 2;

                    if (ctx->normal_attrib_.buffer_obj_ != 0)
                        temp_desc.offset = ctx->normal_attrib_.offset_;
                    else
                        temp_desc.offset = 0;
                    temp_desc.stride = ctx->normal_attrib_.stride_;

                    gl_enum_to_drivers_data_format(ctx->normal_attrib_.data_type_, temp_format);
                    temp_desc.set_format(ctx->normal_attrib_.size_, temp_format);

                    descs.push_back(temp_desc);
                }
            }

            if (active_texs && (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD_ARRAY)) {
                for (std::int32_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
                    if (active_texs & (1 << i) && retrieve_vertex_buffer_slot(ctx->texture_units_[i].coord_attrib_, temp_desc.buffer_slot)) {
                        temp_desc.location = 3 + i;

                        if (ctx->texture_units_[i].coord_attrib_.buffer_obj_ != 0)
                            temp_desc.offset = ctx->texture_units_[i].coord_attrib_.offset_;
                        else
                            temp_desc.offset = 0;

                        temp_desc.stride = ctx->texture_units_[i].coord_attrib_.stride_;

                        gl_enum_to_drivers_data_format(ctx->texture_units_[i].coord_attrib_.data_type_, temp_format);
                        temp_desc.set_format(ctx->texture_units_[i].coord_attrib_.size_, temp_format);

                        descs.push_back(temp_desc);
                    }
                }
            }

            if (!ctx->input_desc_) {
                ctx->input_desc_ = drivers::create_input_descriptors(drv, descs.data(), static_cast<std::uint32_t>(descs.size()));
            } else {
                ctx->command_builder_->update_input_descriptors(ctx->input_desc_, descs.data(), static_cast<std::uint32_t>(descs.size()));
            }

            ctx->command_builder_->set_vertex_buffers(vertex_buffers_alloc.data(), 0, static_cast<std::uint32_t>(vertex_buffers_alloc.size()));
            ctx->attrib_changed_ = false;
        }

        ctx->command_builder_->bind_input_descriptors(ctx->input_desc_);
    }

    static std::uint32_t retrieve_active_textures_bitarr(egl_context_es1 *ctx) {
        std::uint32_t arr = 0;
        if (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_TEXTURE_ENABLE) {
            for (std::size_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
                auto *inst = ctx->objects_.get(ctx->texture_units_[i].binded_texture_handle_);
                if (inst && (*inst).get()) {
                    arr |= (1 << i);
                }
            }
        }

        return arr;
    }

    static bool prepare_gles1_draw(egl_context_es1 *ctx, drivers::graphics_driver *drv, kernel::process *crr_process, const std::uint32_t vcount, dispatch::egl_controller &controller) {
        if ((ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_VERTEX_ARRAY) == 0) {
            // No drawing needed?
            return true;
        }

        std::uint32_t active_textures_bitarr = retrieve_active_textures_bitarr(ctx);
        prepare_vertex_buffer_and_descriptors(ctx, drv, crr_process, vcount, active_textures_bitarr);

        // Active textures
        for (std::int32_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
            if (active_textures_bitarr & (1 << i)) {
                auto *obj = ctx->objects_.get(ctx->texture_units_[i].binded_texture_handle_);

                if (obj)
                    ctx->command_builder_->bind_texture((*obj)->handle_value(), i);
            }
        }

        if (!prepare_shader_program_for_draw(controller, ctx, active_textures_bitarr)) {
            return false;
        }

        return true;
    }

    static bool convert_gl_enum_to_primitive_mode(const std::uint32_t mode, drivers::graphics_primitive_mode &res) {
        switch (mode) {
        case GL_POINTS_EMU:
            res = drivers::graphics_primitive_mode::points;
            break;

        case GL_LINES_EMU:
            res = drivers::graphics_primitive_mode::lines;
            break;

        case GL_LINE_LOOP_EMU:
            res = drivers::graphics_primitive_mode::line_loop;
            break;

        case GL_LINE_STRIP_EMU:
            res = drivers::graphics_primitive_mode::line_strip;
            break;

        case GL_TRIANGLES_EMU:
            res = drivers::graphics_primitive_mode::triangles;
            break;

        case GL_TRIANGLE_STRIP_EMU:
            res = drivers::graphics_primitive_mode::triangle_strip;
            break;

        case GL_TRIANGLE_FAN_EMU:
            res = drivers::graphics_primitive_mode::triangle_fan;
            break;

        default:
            return false;
        }

        return true;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_draw_arrays_emu, std::uint32_t mode, std::int32_t first_index, const std::int32_t count) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        drivers::graphics_driver *drv = sys->get_graphics_driver();

        drivers::graphics_primitive_mode prim_mode_drv;
        if (!convert_gl_enum_to_primitive_mode(mode, prim_mode_drv)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (count < 0) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (!prepare_gles1_draw(ctx, drv, sys->get_kernel_system()->crr_process(), count, controller)) {
            LOG_ERROR(HLE_DISPATCHER, "Error while preparing GLES1 draw. This should not happen!");
            return;
        }

        ctx->command_builder_->draw_arrays(prim_mode_drv, first_index, count, false);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_draw_elements_emu, std::uint32_t mode, std::int32_t count, const std::uint32_t index_type,
        std::uint32_t indices_ptr) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        drivers::graphics_driver *drv = sys->get_graphics_driver();

        drivers::graphics_primitive_mode prim_mode_drv;
        if (!convert_gl_enum_to_primitive_mode(mode, prim_mode_drv)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (count < 0) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (!prepare_gles1_draw(ctx, drv, sys->get_kernel_system()->crr_process(), count, controller)) {
            LOG_ERROR(HLE_DISPATCHER, "Error while preparing GLES1 draw. This should not happen!");
            return;
        }

        drivers::data_format index_format_drv;
        std::size_t size_ibuffer = 0;

        switch (index_type) {
        case GL_UNSIGNED_BYTE_EMU:
            index_format_drv = drivers::data_format::byte;
            size_ibuffer = static_cast<std::size_t>(count);
            break;

        case GL_UNSIGNED_SHORT_EMU:
            index_format_drv = drivers::data_format::word;
            size_ibuffer = static_cast<std::size_t>(count * 2);
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
        
        gles1_driver_buffer *binded_elem_buffer_managed = ctx->binded_buffer(false);

        if (!binded_elem_buffer_managed) {
            // Upload it to a temp buffer (sadly!)
            kernel_system *kern = sys->get_kernel_system();
            const void *indicies_data_raw = kern->crr_process()->get_ptr_on_addr_space(indices_ptr);
            if (!indicies_data_raw) {
                LOG_ERROR(HLE_DISPATCHER, "Interpreting indices pointer as a real pointer, but invalid!");
                controller.push_error(ctx, GL_INVALID_OPERATION);

                return;
            }

            if (!ctx->index_buffer_temp_) {
                ctx->index_buffer_temp_ = drivers::create_buffer(drv, indicies_data_raw, size_ibuffer, static_cast<drivers::buffer_upload_hint>(drivers::buffer_upload_dynamic | drivers::buffer_upload_draw));
                ctx->index_buffer_temp_size_ = size_ibuffer;
            } else {
                if (ctx->index_buffer_temp_size_ < static_cast<std::size_t>(size_ibuffer)) {
                    ctx->command_builder_->recreate_buffer(ctx->index_buffer_temp_, indicies_data_raw, size_ibuffer,
                        static_cast<drivers::buffer_upload_hint>(drivers::buffer_upload_dynamic | drivers::buffer_upload_draw));
                    ctx->index_buffer_temp_size_ = size_ibuffer;
                } else {
                    std::uint32_t size_casted = static_cast<std::uint32_t>(size_ibuffer);
                    ctx->command_builder_->update_buffer_data(ctx->index_buffer_temp_, 0, 1,
                        &indicies_data_raw, &size_casted);
                }
            }

            ctx->command_builder_->set_index_buffer(ctx->index_buffer_temp_);
            indices_ptr = 0;
        } else {
            ctx->command_builder_->set_index_buffer(binded_elem_buffer_managed->handle_value());
        }

        ctx->command_builder_->draw_indexed(prim_mode_drv, count, index_format_drv, 0, indices_ptr);
    }
}