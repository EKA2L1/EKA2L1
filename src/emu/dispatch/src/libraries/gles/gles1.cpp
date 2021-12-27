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

#include <dispatch/libraries/gles/gles1.h>
#include <dispatch/dispatcher.h>
#include <drivers/graphics/graphics.h>
#include <system/epoc.h>
#include <kernel/kernel.h>

namespace eka2l1::dispatch {
    gles_texture_unit::gles_texture_unit()
        : index_(0)
        , binded_texture_handle_(0) {
        texture_mat_stack_.push(glm::identity<glm::mat4>());
    }

    egl_context_es1::egl_context_es1()
        : clear_depth_(0.0f)
        , clear_stencil_(0)
        , active_texture_unit_(0)
        , active_mat_stack_(GL_MODELVIEW_EMU)
        , binded_array_buffer_handle_(0)
        , binded_element_array_buffer_handle_(0)
        , active_cull_face_(drivers::rendering_face::back)
        , active_front_face_rule_(drivers::rendering_face_determine_rule::vertices_counter_clockwise)
        , state_statuses_(0)
        , alpha_test_ref_(0)
        , alpha_test_func_(GL_ALWAYS_EMU)
        , shade_model_(GL_SMOOTH_EMU) {
        clear_color_[0] = 0.0f;
        clear_color_[1] = 0.0f;
        clear_color_[2] = 0.0f;
        clear_color_[3] = 0.0f;

        model_view_mat_stack_.push(glm::identity<glm::mat4>());
        proj_mat_stack_.push(glm::identity<glm::mat4>());

        for (std::size_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
            texture_units_[i].index_ = i;
        }
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
            return surface_fb_;
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
        , internal_format_(0) {
    }

    gles1_driver_buffer::gles1_driver_buffer(egl_context_es1 *ctx)
        : gles1_driver_object(ctx) {
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

    BRIDGE_FUNC_DISPATCHER(void, gl_clear_color_emu, float red, float green, float blue, float alpha) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->clear_color_[0] = red;
        ctx->clear_color_[1] = green;
        ctx->clear_color_[2] = blue;
        ctx->clear_color_[3] = alpha;
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_clear_colorx_emu, std::int32_t red, std::int32_t green, std::int32_t blue, std::int32_t alpha) {
        gl_clear_color_emu(sys, 0, FIXED_32_TO_FLOAT(red), FIXED_32_TO_FLOAT(green), FIXED_32_TO_FLOAT(blue), FIXED_32_TO_FLOAT(alpha));
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_clear_depthf_emu, float depth) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->clear_depth_ = depth;
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_clear_depthx_emu, std::int32_t depth) {
        gl_clear_depthf_emu(sys, 0, FIXED_32_TO_FLOAT(depth));
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_clear_stencil, std::int32_t s) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->clear_stencil_ = s;
    }
    
    BRIDGE_FUNC_DISPATCHER(void, gl_clear_emu, std::uint32_t bits) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        eka2l1::vecx<float, 6> clear_parameters;

        std::uint32_t flags_driver = 0;
        if (bits & GL_DEPTH_BUFFER_BIT_EMU) {
            flags_driver |= drivers::draw_buffer_bit_depth_buffer;

            clear_parameters[0] = ctx->clear_color_[0];
            clear_parameters[1] = ctx->clear_color_[0];
            clear_parameters[2] = ctx->clear_color_[0];
            clear_parameters[3] = ctx->clear_color_[0];
        }

        if (bits & GL_STENCIL_BUFFER_BIT_EMU) {
            flags_driver |= drivers::draw_buffer_bit_stencil_buffer;
            clear_parameters[4] = ctx->clear_depth_;
        }

        if (bits & GL_COLOR_BUFFER_BIT_EMU) {
            flags_driver |= drivers::draw_buffer_bit_stencil_buffer;
            clear_parameters[5] = ctx->clear_stencil_ / 255.0f;
        }

        ctx->command_builder_->clear(clear_parameters, flags_driver);
    }
    
    BRIDGE_FUNC_DISPATCHER(void, gl_matrix_mode_emu, std::uint32_t mode) {
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
    
    BRIDGE_FUNC_DISPATCHER(void, gl_push_matrix_emu) {
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

    BRIDGE_FUNC_DISPATCHER(void, gl_pop_matrix_emu) {
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
    
    BRIDGE_FUNC_DISPATCHER(void, gl_load_identity_emu) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::identity<glm::mat4>();
    }
    
    BRIDGE_FUNC_DISPATCHER(void, gl_load_matrixf_emu, float *mat) {
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

    BRIDGE_FUNC_DISPATCHER(void, gl_load_matrixf_emu, std::uint32_t *mat) {
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
    
    BRIDGE_FUNC_DISPATCHER(std::uint32_t, gl_get_error_emu) {
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
    
    BRIDGE_FUNC_DISPATCHER(void, gl_orthof_emu, float left, float right, float bottom, float top, float near, float far) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() *= glm::ortho(left, right, bottom, top, near, far);
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_orthox_emu, std::uint32_t left, std::uint32_t right, std::uint32_t bottom, std::uint32_t top, std::uint32_t near, std::uint32_t far) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() *= glm::ortho(FIXED_32_TO_FLOAT(left), FIXED_32_TO_FLOAT(right), FIXED_32_TO_FLOAT(bottom), FIXED_32_TO_FLOAT(top),
            FIXED_32_TO_FLOAT(near), FIXED_32_TO_FLOAT(far));
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_mult_matrixf_emu, float *m) {
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

    BRIDGE_FUNC_DISPATCHER(void, gl_mult_matrixx_emu, std::uint32_t *m) {
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
    
    BRIDGE_FUNC_DISPATCHER(void, gl_scalef_emu, float x, float y, float z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::scale(ctx->active_matrix(), glm::vec3(x, y, z));
    }
    
    BRIDGE_FUNC_DISPATCHER(void, gl_scalex_emu, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::scale(ctx->active_matrix(), glm::vec3(FIXED_32_TO_FLOAT(x), FIXED_32_TO_FLOAT(y), FIXED_32_TO_FLOAT(z)));
    }
    
    BRIDGE_FUNC_DISPATCHER(void, gl_translatef_emu, float x, float y, float z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::translate(ctx->active_matrix(), glm::vec3(x, y, z));
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_translatex_emu, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::translate(ctx->active_matrix(), glm::vec3(FIXED_32_TO_FLOAT(x), FIXED_32_TO_FLOAT(y), FIXED_32_TO_FLOAT(z)));
    }
    
    BRIDGE_FUNC_DISPATCHER(void, gl_rotatef_emu, float angles, float x, float y, float z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::rotate(ctx->active_matrix(), angles, glm::vec3(x, y, z));
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_rotatex_emu, std::uint32_t angles, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::rotate(ctx->active_matrix(), FIXED_32_TO_FLOAT(angles), glm::vec3(FIXED_32_TO_FLOAT(x), FIXED_32_TO_FLOAT(y), FIXED_32_TO_FLOAT(z)));
    }
    
    BRIDGE_FUNC_DISPATCHER(void, gl_frustumf_emu, float left, float right, float bottom, float top, float near, float far) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() *= glm::frustum(left, right, bottom, top, near, far);
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_frustumx_emu, std::uint32_t left, std::uint32_t right, std::uint32_t bottom, std::uint32_t top, std::uint32_t near, std::uint32_t far) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() *= glm::frustum(FIXED_32_TO_FLOAT(left), FIXED_32_TO_FLOAT(right), FIXED_32_TO_FLOAT(bottom), FIXED_32_TO_FLOAT(top),
            FIXED_32_TO_FLOAT(near), FIXED_32_TO_FLOAT(far));
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_cull_face_emu, std::uint32_t mode) {
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
    
    BRIDGE_FUNC_DISPATCHER(void, gl_scissor_emu, std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        // Emulator graphics abstraction use top-left as origin.
        ctx->command_builder_->clip_rect(eka2l1::rect(eka2l1::vec2(x, ctx->dimension_.y - y), eka2l1::vec2(width, height)));
    }
    
    BRIDGE_FUNC_DISPATCHER(void, gl_front_face_emu, std::uint32_t mode) {
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
    
    BRIDGE_FUNC_DISPATCHER(bool, gl_is_texture_emu, std::uint32_t name) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return false;
        }

        gles1_driver_object_instance *obj = ctx->objects_.get(name);
        return obj && ((*obj)->object_type() == GLES1_OBJECT_TEXTURE);
    }

    BRIDGE_FUNC_DISPATCHER(bool, gl_is_buffer_emu, std::uint32_t name) {
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

    BRIDGE_FUNC_DISPATCHER(void, gl_gen_textures_emu, std::int32_t n, std::uint32_t *texs) {
        gen_gles1_objects_generic(sys, GLES1_OBJECT_TEXTURE, n, texs);
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_gen_buffers_emu, std::int32_t n, std::uint32_t *buffers) {
        gen_gles1_objects_generic(sys, GLES1_OBJECT_BUFFER, n, buffers);
    }
    
    BRIDGE_FUNC_DISPATCHER(void, gl_delete_textures_emu, std::int32_t n, std::uint32_t *texs) {
        delete_gles1_objects_generic(sys, GLES1_OBJECT_TEXTURE, n, texs);
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_delete_buffers_emu, std::int32_t n, std::uint32_t *buffers) {
        delete_gles1_objects_generic(sys, GLES1_OBJECT_BUFFER, n, buffers);
    }
    
    BRIDGE_FUNC_DISPATCHER(void, gl_bind_texture_emu, std::uint32_t target, std::uint32_t name) {
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

    BRIDGE_FUNC_DISPATCHER(void, gl_bind_buffer_emu, std::uint32_t target, std::uint32_t name) {
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
    
    BRIDGE_FUNC_DISPATCHER(void, gl_alpha_func_emu, std::uint32_t func, float ref) {
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

        ctx->alpha_test_func_ = func;
        ctx->alpha_test_ref_ = ref;
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_alpha_func_x_emu, std::uint32_t func, std::uint32_t ref) {
        gl_alpha_func_emu(sys, 0, func, FIXED_32_TO_FLOAT(ref));
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
            format_out = drivers::texture_format::rgb;
            internal_out = format_out;
            swizzles_out = { drivers::channel_swizzle::red, drivers::channel_swizzle::green, drivers::channel_swizzle::blue, drivers::channel_swizzle::one };

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

    BRIDGE_FUNC_DISPATCHER(void, gl_tex_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t internal_format,
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

        ctx->command_builder_->set_swizzle(tex->handle_value(), swizzles[0], swizzles[1], swizzles[2], swizzles[3]);
    }
    
    BRIDGE_FUNC_DISPATCHER(void, gl_tex_sub_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t xoffset,
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
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_shade_model_emu, std::uint32_t model) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((model != GL_FLAT_EMU) && (model != GL_SMOOTH_EMU)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->shade_model_ = model;
    }
    
    BRIDGE_FUNC_DISPATCHER(void, gl_normal_3f_emu, float nx, float ny, float nz) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->normal_uniforms_[0] = nx;
        ctx->normal_uniforms_[1] = ny;
        ctx->normal_uniforms_[2] = nz;
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_normal_3x_emu, std::uint32_t nx, std::uint32_t ny, std::uint32_t nz) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->normal_uniforms_[0] = FIXED_32_TO_FLOAT(nx);
        ctx->normal_uniforms_[1] = FIXED_32_TO_FLOAT(ny);
        ctx->normal_uniforms_[2] = FIXED_32_TO_FLOAT(nz);
    }
    
    BRIDGE_FUNC_DISPATCHER(void, gl_color_4f_emu, float red, float green, float blue, float alpha) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->color_uniforms_[0] = red;
        ctx->color_uniforms_[1] = green;
        ctx->color_uniforms_[2] = blue;
        ctx->color_uniforms_[3] = alpha;
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_color_4x_emu, std::uint32_t red, std::uint32_t green, std::uint32_t blue, std::uint32_t alpha) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->color_uniforms_[0] = FIXED_32_TO_FLOAT(red);
        ctx->color_uniforms_[1] = FIXED_32_TO_FLOAT(green);
        ctx->color_uniforms_[2] = FIXED_32_TO_FLOAT(blue);
        ctx->color_uniforms_[3] = FIXED_32_TO_FLOAT(alpha);
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_color_4ub_emu, std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->color_uniforms_[0] = red / 255.0f;
        ctx->color_uniforms_[1] = green / 255.0f;
        ctx->color_uniforms_[2] = blue / 255.0f;
        ctx->color_uniforms_[3] = alpha / 255.0f;
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_active_texture_emu, std::uint32_t unit) {
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
    
    BRIDGE_FUNC_DISPATCHER(void, gl_client_active_texture_emu, std::uint32_t unit) {
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
    
    BRIDGE_FUNC_DISPATCHER(void, gl_multi_tex_coord_4f_emu, std::uint32_t unit, float s, float t, float r, float q) {
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

    BRIDGE_FUNC_DISPATCHER(void, gl_multi_tex_coord_4x_emu, std::uint32_t unit, std::uint32_t s, std::uint32_t t, std::uint32_t r, std::uint32_t q) {
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

    BRIDGE_FUNC_DISPATCHER(void, gl_enable_client_state_emu, std::uint32_t state) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (state) {
        case GL_VERTEX_ARRAY_EMU:
            ctx->state_statuses_ |= egl_context_es1::STATE_CLIENT_VERTEX_ARRAY;
            break;

        case GL_COLOR_ARRAY_EMU:
            ctx->state_statuses_ |= egl_context_es1::STATE_CLIENT_COLOR_ARRAY;
            break;

        case GL_NORMAL_ARRAY_EMU:
            ctx->state_statuses_ |= egl_context_es1::STATE_CLIENT_NORMAL_ARRAY;
            break;

        case GL_TEXTURE_COORD_ARRAY_EMU:
            ctx->state_statuses_ |= egl_context_es1::STATE_CLIENT_TEXCOORD_ARRAY;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_DISPATCHER(void, gl_disable_client_state_emu, std::uint32_t state) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (state) {
        case GL_VERTEX_ARRAY_EMU:
            ctx->state_statuses_ &= ~egl_context_es1::STATE_CLIENT_VERTEX_ARRAY;
            break;

        case GL_COLOR_ARRAY_EMU:
            ctx->state_statuses_ &= ~egl_context_es1::STATE_CLIENT_COLOR_ARRAY;
            break;

        case GL_NORMAL_ARRAY_EMU:
            ctx->state_statuses_ &= ~egl_context_es1::STATE_CLIENT_NORMAL_ARRAY;
            break;

        case GL_TEXTURE_COORD_ARRAY_EMU:
            ctx->state_statuses_ &= ~egl_context_es1::STATE_CLIENT_TEXCOORD_ARRAY;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }
}