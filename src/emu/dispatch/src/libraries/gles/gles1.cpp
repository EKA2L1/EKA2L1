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
    static bool is_gles1_driver_object_free(gles1_driver_object &obj) {
        return obj.second == 0;
    }

    void free_gles1_driver_object_from_container(gles1_driver_object &obj) {
        obj.second = 0;
    }

    egl_context_es1::egl_context_es1()
        : clear_depth_(0.0f)
        , clear_stencil_(0)
        , active_mat_stack_(GL_MODELVIEW_EMU)
        , active_cull_face_(drivers::rendering_face::back)
        , active_front_face_rule_(drivers::rendering_face_determine_rule::vertices_counter_clockwise)
        , objects_(is_gles1_driver_object_free, free_gles1_driver_object_from_container)
        , binded_texture_handle_(0)
        , binded_array_buffer_handle_(0)
        , binded_element_array_buffer_handle_(0) {
        clear_color_[0] = 0.0f;
        clear_color_[1] = 0.0f;
        clear_color_[2] = 0.0f;
        clear_color_[3] = 0.0f;

        model_view_mat_stack_.push(glm::identity<glm::mat4>());
        proj_mat_stack_.push(glm::identity<glm::mat4>());
        texture_mat_stack_.push(glm::identity<glm::mat4>());
    }

    glm::mat4 &egl_context_es1::active_matrix() {
        switch (active_mat_stack_) {
        case GL_MODELVIEW_EMU:
            return model_view_mat_stack_.top();

        case GL_PROJECTION_EMU:
            return proj_mat_stack_.top();

        case GL_TEXTURE_EMU:
            return texture_mat_stack_.top();

        default:
            break;
        }

        return model_view_mat_stack_.top();
    }

    drivers::handle *egl_context_es1::binded_texture_driver_handle() {
        if (binded_texture_handle_ == 0) {
            return &surface_fb_;
        }

        auto *obj = objects_.get(binded_texture_handle_);
        if (!obj || obj->first != GLES1_OBJECT_TEXTURE) {
            return nullptr;
        }

        return &(obj->second);
    }

    drivers::handle *egl_context_es1::binded_buffer_driver_handle(const bool is_array_buffer) {
        gles1_driver_object *obj = objects_.get(is_array_buffer ? binded_array_buffer_handle_ : binded_element_array_buffer_handle_);
        if (!obj) {
            return nullptr;
        }

        return (&obj->second);
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

        case GL_TEXTURE_EMU:
            if (ctx->texture_mat_stack_.size() >= GL_MAXIMUM_TEXTURE_MATRIX_STACK_SIZE) {
                controller.push_error(ctx, GL_STACK_OVERFLOW);
                return;
            }

            ctx->texture_mat_stack_.push(ctx->texture_mat_stack_.top());
            break;
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
            if (ctx->texture_mat_stack_.size() == 1) {
                controller.push_error(ctx, GL_STACK_UNDERFLOW);
                return;
            }

            ctx->texture_mat_stack_.pop();
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

        gles1_driver_object *obj = ctx->objects_.get(name);
        return obj && (obj->first == GLES1_OBJECT_TEXTURE);
    }

    BRIDGE_FUNC_DISPATCHER(bool, gl_is_buffer_emu, std::uint32_t name) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return false;
        }

        gles1_driver_object *obj = ctx->objects_.get(name);
        return obj && (obj->first == GLES1_OBJECT_BUFFER);
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

        gles1_driver_object stub_obj;

        for (std::int32_t i = 0; i < n; i++) {
            stub_obj.first = obj_type;
            stub_obj.second = static_cast<kernel::handle>(GLES1_UNINITIALIZED_DRIVER_HANDLE);

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

        bool need_free = false;

        auto cmd_list = drv->new_command_list();
        auto cmd_builder = drv->new_command_builder(cmd_list.get());

        for (std::int32_t i = 0; i < n; i++) {
            auto *obj = ctx->objects_.get(names[i]);
            if (obj && (obj->first == obj_type)) {
                if (obj->second != GLES1_UNINITIALIZED_DRIVER_HANDLE) {
                    cmd_builder->destroy(obj->second);
                    need_free = true;
                }

                ctx->objects_.remove(names[i]);
            }
        }

        if (need_free) {
            drv->submit_command_list(*cmd_list);
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

        ctx->binded_texture_handle_ = name;
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
}