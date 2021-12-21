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

#pragma once

#include <dispatch/libraries/gles/def.h>
#include <dispatch/def.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace eka2l1 {
    class system;
}

namespace eka2l1::dispatch {
    #define FIXED_32_TO_FLOAT(x) (float)(x) / 65536.0f

    struct egl_context_es1 : public egl_context {
        std::unique_ptr<drivers::graphics_command_list> command_list_;
        std::unique_ptr<drivers::server_graphics_command_list_builder> command_builder_;

        float clear_color_[4];
        float clear_depth_;
        std::int32_t clear_stencil_;

        std::stack<glm::mat4> model_view_mat_stack_;
        std::stack<glm::mat4> proj_mat_stack_;
        std::stack<glm::mat4> texture_mat_stack_;

        std::uint32_t active_mat_stack_;

        explicit egl_context_es1();
        glm::mat4 &active_matrix();

        void free(drivers::graphics_driver *driver, drivers::graphics_command_list_builder &builder) override;
        egl_context_type context_type() const override {
            return EGL_GLES1_CONTEXT;
        }
    };

    egl_context_es1 *get_es1_active_context(system *sys);

    BRIDGE_FUNC_DISPATCHER(void, gl_clear_color_emu, float red, float green, float blue, float alpha);
    BRIDGE_FUNC_DISPATCHER(void, gl_clear_colorx_emu, std::int32_t red, std::int32_t green, std::int32_t blue, std::int32_t alpha);
    BRIDGE_FUNC_DISPATCHER(void, gl_clear_depthf_emu, float depth);
    BRIDGE_FUNC_DISPATCHER(void, gl_clear_depthx_emu, std::int32_t depth);
    BRIDGE_FUNC_DISPATCHER(void, gl_clear_stencil, std::int32_t s);
    BRIDGE_FUNC_DISPATCHER(void, gl_clear_emu, std::uint32_t bits);
    BRIDGE_FUNC_DISPATCHER(void, gl_matrix_mode_emu, std::uint32_t mode);
    BRIDGE_FUNC_DISPATCHER(void, gl_push_matrix_emu);
    BRIDGE_FUNC_DISPATCHER(void, gl_pop_matrix_emu);
    BRIDGE_FUNC_DISPATCHER(void, gl_load_identity_emu, float *mat);
    BRIDGE_FUNC_DISPATCHER(void, gl_load_matrixf_emu, std::uint32_t *mat);
    BRIDGE_FUNC_DISPATCHER(std::uint32_t, gl_get_error_emu);
    BRIDGE_FUNC_DISPATCHER(void, gl_orthof_emu, float left, float right, float bottom, float top, float near, float far);
    BRIDGE_FUNC_DISPATCHER(void, gl_orthox_emu, std::uint32_t left, std::uint32_t right, std::uint32_t bottom, std::uint32_t top, std::uint32_t near, std::uint32_t far);
    BRIDGE_FUNC_DISPATCHER(void, gl_mult_matrixf_emu, float *m);
    BRIDGE_FUNC_DISPATCHER(void, gl_mult_matrixx_emu, std::uint32_t *m);
    BRIDGE_FUNC_DISPATCHER(void, gl_scalef_emu, float x, float y, float z);
    BRIDGE_FUNC_DISPATCHER(void, gl_scalex_emu, std::uint32_t x, std::uint32_t y, std::uint32_t z);
    BRIDGE_FUNC_DISPATCHER(void, gl_translatef_emu, float x, float y, float z);
    BRIDGE_FUNC_DISPATCHER(void, gl_translatex_emu, std::uint32_t x, std::uint32_t y, std::uint32_t z);
    BRIDGE_FUNC_DISPATCHER(void, gl_rotatef_emu, float angles, float x, float y, float z);
    BRIDGE_FUNC_DISPATCHER(void, gl_rotatex_emu, std::uint32_t angles, std::uint32_t x, std::uint32_t y, std::uint32_t z);
    BRIDGE_FUNC_DISPATCHER(void, gl_frustumf_emu, float left, float right, float bottom, float top, float near, float far);
    BRIDGE_FUNC_DISPATCHER(void, gl_frustumx_emu, std::uint32_t left, std::uint32_t right, std::uint32_t bottom, std::uint32_t top, std::uint32_t near, std::uint32_t far);
}