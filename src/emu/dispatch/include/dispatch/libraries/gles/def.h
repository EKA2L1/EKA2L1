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
        GL_TEXTURE_2D_EMU = 0x0DE1,
        GL_DEPTH_BUFFER_BIT_EMU = 0x00000100,
        GL_STENCIL_BUFFER_BIT_EMU = 0x00000400,
        GL_COLOR_BUFFER_BIT_EMU = 0x00004000,
        GL_MODELVIEW_EMU =  0x1700,
        GL_PROJECTION_EMU = 0x1701,
        GL_TEXTURE_EMU = 0x1702,
        GL_ARRAY_BUFFER_EMU = 0x8892,
        GL_ELEMENT_ARRAY_BUFFER_EMU = 0x8893,
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