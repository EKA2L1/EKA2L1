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

#include <drivers/graphics/common.h>
#include <drivers/itc.h>

#include <common/vecx.h>
#include <common/container.h>

#include <kernel/common.h>

#include <cstdint>
#include <memory>
#include <map>
#include <stack>

namespace eka2l1 {
    namespace epoc {
        struct screen;
    }

    namespace drivers {
        class graphics_driver;
    }
}

namespace eka2l1::dispatch {
    enum {
        GL_NO_ERROR = 0,
        GL_ZERO_EMU = 0,
        GL_ONE_EMU = 1,
        GL_NEVER_EMU = 0x0200,
        GL_LESS_EMU = 0x0201,
        GL_EQUAL_EMU = 0x0202,
        GL_LEQUAL_EMU = 0x0203,
        GL_GREATER_EMU = 0x0204,
        GL_NOTEQUAL_EMU = 0x0205,
        GL_GEQUAL_EMU = 0x0206,
        GL_ALWAYS_EMU = 0x0207,
        GL_FRONT_EMU = 0x0404,
        GL_BACK_EMU = 0x0405,
        GL_FRONT_AND_BACK_EMU = 0x0408,
        GL_INVALID_ENUM = 0x0500,
        GL_INVALID_VALUE = 0x0501,
        GL_INVALID_OPERATION = 0x0502,
        GL_STACK_OVERFLOW = 0x0503,
        GL_STACK_UNDERFLOW = 0x0504,
        GL_CLOCKWISE_EMU = 0x0900,
        GL_COUNTER_CLOCKWISE_EMU = 0x0901,
        GL_INVALID_FRAMEBUFFER_OPERATION = 0x0506,
        GL_EXP_EMU = 0x800,
        GL_EXP2_EMU = 0x801,
        GL_FOG_DENSITY_EMU = 0x0B62,
        GL_FOG_START_EMU = 0x0B63,
        GL_FOG_END_EMU = 0x0B64,
        GL_FOG_MODE_EMU = 0x0B65,
        GL_FOG_COLOR_EMU = 0x0B66,
        GL_BLEND_EMU = 0x0BE2,
        GL_TEXTURE_2D_EMU = 0x0DE1,
        GL_DEPTH_BUFFER_BIT_EMU = 0x00000100,
        GL_STENCIL_BUFFER_BIT_EMU = 0x00000400,
        GL_COLOR_BUFFER_BIT_EMU = 0x00004000,
        GL_AMBIENT_EMU = 0x1200,
        GL_DIFFUSE_EMU = 0x1201,
        GL_SPECULAR_EMU = 0x1202,
        GL_UNSIGNED_BYTE_EMU = 0x1401,
        GL_EMISSION_EMU = 0x1600,
        GL_SHININESS_EMU = 0x1601,
        GL_AMBIENT_AND_DIFFUSE_EMU = 0x1602,
        GL_MODELVIEW_EMU = 0x1700,
        GL_PROJECTION_EMU = 0x1701,
        GL_TEXTURE_EMU = 0x1702,
        GL_ALPHA_EMU = 0x1906,
        GL_RGB_EMU = 0x1907,
        GL_RGBA_EMU = 0x1908,
        GL_LUMINANCE_EMU = 0x1909,
        GL_LUMINANCE_ALPHA_EMU = 0x190A,
        GL_FLAT_EMU = 0x1D00,
        GL_SMOOTH_EMU = 0x1D01,
        GL_NEAREST_EMU = 0x2600,
        GL_LINEAR_EMU = 0x2601,
        GL_SRC_COLOR_EMU = 0x0300,
        GL_ONE_MINUS_SRC_COLOR_EMU = 0x0301,
        GL_SRC_ALPHA_EMU = 0x0302,
        GL_ONE_MINUS_SRC_ALPHA_EMU = 0x0303,
        GL_DST_ALPHA_EMU = 0x0304,
        GL_ONE_MINUS_DST_ALPHA_EMU = 0x305,
        GL_DST_COLOR = 0x0306,
        GL_ONE_MINUS_DST_COLOR = 0x0307,
        GL_SRC_ALPHA_SATURATE_EMU = 0x308,
        GL_MODULATE_EMU = 0x2100,
        GL_DECAL_EMU = 0x2101,
        GL_ADD_EMU = 0x0104,
        GL_REPLACE_EMU = 0x1E01,
        GL_TEX_ENV_MODE_EMU = 0x2200,
        GL_TEX_ENV_COLOR_EMU = 0x2201,
        GL_TEX_ENV_EMU = 0x2300,
        GL_UNSIGNED_SHORT_4_4_4_4_EMU = 0x8033,
        GL_UNSIGNED_SHORT_5_5_5_1_EMU = 0x8034,
        GL_UNSIGNED_SHORT_5_6_5_EMU = 0x8363,
        GL_VERTEX_ARRAY_EMU = 0x8074,
        GL_NORMAL_ARRAY_EMU = 0x8075,
        GL_COLOR_ARRAY_EMU = 0x8076,
        GL_TEXTURE_COORD_ARRAY_EMU = 0x8078,
        GL_TEXTURE0_EMU = 0x84C0,
        GL_TEXTURE1_EMU = 0x84C1,
        GL_TEXTURE2_EMU = 0x84C2,
        GL_ARRAY_BUFFER_EMU = 0x8892,
        GL_ELEMENT_ARRAY_BUFFER_EMU = 0x8893,
        GL_STATIC_DRAW_EMU = 0x88E4,
        GL_DYNAMIC_DRAW_EMU = 0x88E8,
        GL_SUBTRACT_EMU = 0x84E7,
        GL_COMBINE_EMU = 0x8570,
        GL_COMBINE_RGB_EMU = 0x8571,
        GL_COMBINE_ALPHA_EMU = 0x8572,
        GL_RGB_SCALE_EMU = 0x8573,
        GL_ADD_SIGNED_EMU = 0x8574,
        GL_INTERPOLATE_EMU = 0x8575,
        GL_CONSTANT_EMU = 0x8576,
        GL_PRIMARY_COLOR_EMU = 0x8577,
        GL_PREVIOUS_EMU = 0x8578,
        GL_OPERAND0_RGB_EMU = 0x8590,
        GL_OPERAND1_RGB_EMU = 0x8591,
        GL_OPERAND2_RGB_EMU = 0x8592,
        GL_OPERAND0_ALPHA_EMU = 0x8598,
        GL_OPERAND1_ALPHA_EMU = 0x8599,
        GL_OPERAND2_ALPHA_EMU = 0x859A,
        GL_ALPHA_SCALE_EMU = 0x0D1C,
        GL_SRC0_RGB_EMU = 0x8580,
        GL_SRC1_RGB_EMU = 0x8581,
        GL_SRC2_RGB_EMU = 0x8582,
        GL_SRC0_ALPHA_EMU = 0x8588,
        GL_SRC1_ALPHA_EMU = 0x8589,
        GL_SRC2_ALPHA_EMU = 0x858A,
        GL_DOT3_RGB_EMU = 0x86AE,
        GL_DOT3_RGBA_EMU = 0x86AF,
        GL_MAXIMUM_MODELVIEW_MATRIX_STACK_SIZE = 16,
        GL_MAXIMUM_PROJECTION_MATRIX_STACK_SIZE = 2,
        GL_MAXIMUM_TEXTURE_MATRIX_STACK_SIZE = 2
    };

    using egl_display = std::uint32_t;

    enum egl_context_type {
        EGL_GLES1_CONTEXT,
        EGL_GLES2_CONTEXT
    };

    struct egl_context {
        drivers::handle surface_fb_;
        eka2l1::vec2 dimension_;
        epoc::screen *backed_screen_;       // Use for indicating the emulator scale.

        kernel::uid associated_thread_uid_;

        explicit egl_context();

        virtual void free(drivers::graphics_driver *driver, drivers::graphics_command_list_builder &builder);
        virtual egl_context_type context_type() const = 0;
    };

    using egl_context_instance = std::unique_ptr<egl_context>;
    using egl_context_handle = std::uint32_t;

    struct egl_controller {
    private:
        common::identity_container<egl_context_instance> contexts_;
        std::map<kernel::uid, egl_context *> active_context_;
        std::map<kernel::uid, std::uint32_t> error_map_;

        drivers::graphics_driver *driver_;

    public:
        explicit egl_controller();
        ~egl_controller();

        bool make_current(kernel::uid thread_id, const egl_context_handle handle);
        void clear_current(kernel::uid thread_id);

        egl_context_handle add_context(egl_context_instance &instance);
        void remove_context(const egl_context_handle handle);

        egl_context *current_context(kernel::uid thread_id);

        void push_error(kernel::uid thread_id, const std::uint32_t error);
        std::uint32_t pop_error(kernel::uid thread_id);

        void push_error(egl_context *context, const std::uint32_t error);
        std::uint32_t pop_error(egl_context *context);
    };
}