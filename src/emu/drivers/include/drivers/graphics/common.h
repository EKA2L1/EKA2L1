/*
 * Copyright (c) 2019 EKA2L1 Team
 * Copyright 2018 Dolphin Emulator Project
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

#include <array>
#include <cstdint>

namespace eka2l1::drivers {
    using handle = std::uint64_t;

    enum class graphic_api {
        opengl,
        vulkan
    };

    class graphics_object {
    public:
        virtual ~graphics_object() = default;

    private:
        virtual int holder() {
            return 0;
        }
    };

    enum class graphics_primitive_mode : std::uint8_t {
        points,
        lines,
        line_loop,
        line_strip,
        triangles,
        triangle_strip,
        triangle_fan
    };

    enum class data_format : std::uint8_t {
        byte = 0,
        sbyte = 1,
        word = 2,
        sword = 3,
        sfloat = 4,
        uint = 5,
        sint = 6,
        fixed = 7
    };

    enum class blend_equation : std::uint8_t {
        add,
        sub,
        isub
    };

    enum class blend_factor : std::uint32_t {
        one = 0,
        zero = 1,
        frag_out_alpha = 2,
        one_minus_frag_out_alpha = 3,
        current_alpha = 4,
        one_minus_current_alpha = 5,
        frag_out_color = 6,
        one_minus_frag_out_color = 7,
        current_color = 8,
        one_minus_current_color = 9,
        frag_out_alpha_saturate = 10,
        constant_colour = 11,
        one_minus_constant_colour = 12,
        constant_alpha = 13,
        one_minus_constant_alpha = 14
    };

    enum draw_buffer_bits : std::uint32_t  {
        draw_buffer_bit_color_buffer = 1 << 0,
        draw_buffer_bit_depth_buffer = 1 << 1,
        draw_buffer_bit_stencil_buffer = 1 << 2
    };

    enum bitmap_draw_flags : std::uint32_t {
        bitmap_draw_flag_use_brush = 1 << 0,
        bitmap_draw_flag_invert_mask = 1 << 1,
        bitmap_draw_flag_flip = 1 << 2,
        bitmap_draw_flag_flat_blending = 1 << 3,
        bitmap_draw_flag_use_upscale_shader = 1 << 4        // Only apply to non-mask draw
    };

    enum pen_style : std::uint8_t {
        pen_style_none = 0,
        pen_style_solid = 1,
        pen_style_dotted = 2,
        pen_style_dashed = 3,
        pen_style_dashed_dot = 4,
        pen_style_dashed_dot_dot = 5
    };

    enum class texture_format : std::uint16_t {
        none,
        r,
        r8,
        rg,
        rg8,
        rgb,
        bgr,
        bgra,
        rgba,
        rgba4,
        rgb5_a1,
        rgb565,
        depth16,
        stencil8,
        depth_stencil,
        depth24_stencil8,
        etc2_rgb8,
        pvrtc_4bppv1_rgb,
        pvrtc_2bppv1_rgb,
        pvrtc_4bppv1_rgba,
        pvrtc_2bppv1_rgba
    };

    enum class texture_data_type : std::uint16_t {
        compressed,
        ubyte,
        ushort,
        ushort_4_4_4_4,
        ushort_5_6_5,
        ushort_5_5_5_1,
        uint_24_8
    };

    enum class filter_option : std::uint16_t {
        linear,
        nearest,
        nearest_mipmap_nearest,
        nearest_mipmap_linear,
        linear_mipmap_nearest,
        linear_mipmap_linear
    };

    enum class addressing_option : std::uint16_t {
        clamp_to_edge,
        repeat,
        mirrored_repeat
    };

    enum class addressing_direction : std::uint16_t {
        s,
        t,
        r
    };

    enum class channel_swizzle : std::uint16_t {
        red,
        green,
        blue,
        alpha,
        zero,
        one
    };

    enum class stencil_action : std::uint16_t {
        keep,
        replace,
        invert,
        increment,
        increment_wrap,
        decrement,
        decrement_wrap,
        set_to_zero,
    };

    enum class rendering_face : std::uint16_t {
        back = 0x1,
        front = 0x2,
        back_and_front = back | front
    };

    enum class rendering_face_determine_rule : std::uint16_t {
        vertices_clockwise,
        vertices_counter_clockwise
    };

    enum class condition_func : std::uint16_t {
        never,
        less,
        less_or_equal,
        greater,
        greater_or_equal,
        equal,
        not_equal,
        always
    };

    enum class graphics_feature : std::uint32_t {
        depth_test = 0,
        stencil_test = 1,
        clipping = 2,
        blend = 3,
        cull = 4,
        line_smooth = 5,
        multisample = 6,
        polygon_offset_fill = 7,
        sample_alpha_to_coverage = 8,
        sample_alpha_to_one = 9,
        sample_coverage = 10,
        dither = 11
    };

    enum class shader_module_type : std::uint32_t {
        vertex,
        fragment,
        geometry
    };

    enum framebuffer_bind_type {
        framebuffer_bind_draw = 1 << 0,
        framebuffer_bind_read = 1 << 1,
        framebuffer_bind_read_draw = framebuffer_bind_read | framebuffer_bind_draw
    };

    enum class window_api {
        glfw
    };

    enum class window_system_type {
        headless,
        windows,
        macOS,
        android,
        x11,
        wayland,
        fbDev,
        haiku,
    };

    struct window_system_info {
        window_system_info() = default;

        window_system_info(window_system_type type_, void* display_connection_, void* render_window_, void* render_surface_)
            : type(type_), display_connection(display_connection_), render_window(render_window_),
                render_surface(render_surface_) {
        }

        // Window system type. Determines which GL context or Vulkan WSI is used.
        window_system_type type = window_system_type::headless;

        // Connection to a display server. This is used on X11 and Wayland platforms.
        void* display_connection = nullptr;

        // Render window. This is a pointer to the native window handle, which depends
        // on the platform. e.g. HWND for Windows, Window for X11. If the surface is
        // set to nullptr, the video backend will run in headless mode.
        void* render_window = nullptr;

        // Render surface. Depending on the host platform, this may differ from the window.
        // This is kept seperate as input may require a different handle to rendering, and
        // during video backend startup the surface pointer may change (MoltenVK).
        void* render_surface = nullptr;

        // Scale of the render surface. For hidpi systems, this will be >1.
        float render_surface_scale = 1.0f;
    };

    using channel_swizzles = std::array<channel_swizzle, 4>;
}
