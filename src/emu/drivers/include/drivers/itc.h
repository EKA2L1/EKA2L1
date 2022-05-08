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
#include <common/region.h>

#include <drivers/driver.h>
#include <drivers/graphics/buffer.h>
#include <drivers/graphics/common.h>
#include <drivers/graphics/input_desc.h>
#include <drivers/graphics/shader.h>
#include <drivers/graphics/texture.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace eka2l1::drivers {
    struct command_list;
    class graphics_driver;

    /**
     * BPP to texture format table:
     * 
     * BPP in EKA2L1 follows exactly the order stored on Symbian. The general rules always will be:
     * 
     * - RGB components order will be consequencely placed from high bits to low bits
     * - If the BPP supports alpha, the alpha will be on top of RGB. So the order is always ARGB
     * 
     * For OpenGL (format, internal format, upload data type, swizzle (optional)):
     * 
     * - 8bits: GL_R, GL_R8, GL_BYTE, swizzle RRRR
     * - 12bits: GL_RGBA, GL_RGBA4, GL_UNSIGNED_SHORT_4_4_4_4, swizzle GBA1
     * - 16bits: GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, swizzle RGB1
     * - 24bits: GL_RGB, GL_RGB, GL_BYTE, swizzle BGR1
     * - 32bits: GL_BGRA, GL_BGRA, GL_BYTE, swizzle RGBA
     */

    using graphics_driver_dialog_callback = std::function<void(const char *)>;

    /** \brief Create a new bitmap in the server size.
      *
      * A bitmap will be created in the server side when using this function. When you bind
      * a bitmap for rendering, a render target will be automatically created for it.
      *
      * \param initial_size       The initial size of the bitmap.
      * \param bpp                Bits per pixel of the bitmap
      * 
      * \returns ID of the bitmap.
      */
    drivers::handle create_bitmap(graphics_driver *driver, const eka2l1::vec2 &size, const std::uint32_t bpp);

    /** \brief Create a new shader module.
      *
      * \param driver       The driver associated with the program. 
      * \param data         Pointer to the vertex/fragment shader code.
      * \param size         Size of vertex/fragment shader code in bytes.
      * \param type         The type of the shader module.
      * \param compile_log  Pointer to an optional string object, that hold compile log. It may be important on compile failure.
      *
      * \returns Handle to the program.
      */
    drivers::handle create_shader_module(graphics_driver *driver, const char *data, const std::size_t size,
        drivers::shader_module_type type, std::string *compile_log = nullptr);

    /**
     * @brief Create a shader program from existing shader modules.
     * 
     * @param driver            The driver associated with the program. 
     * @param vertex_module     Handle to the vertex shader module created.
     * @param fragment_module   Handle to the fragment shader module created.
     * @param metadata          If this is not null, the metadata object is filled with this shader program's metadata.
     * @param link_log          If this is not null, on return the log is filled with linking info.
     * 
     * @return A valid handle on success.
     */
    drivers::handle create_shader_program(graphics_driver *driver, drivers::handle vertex_module,
        drivers::handle fragment_module, shader_program_metadata *metadata, std::string *link_log = nullptr);

    /**
     * \brief Create a new texture.
     *
     * \param driver            The driver associated with the texture.
     * \param dim               Total dimensions of this texture.
     * \param mip_levels        Total mips that the data contains.
     * \param internal_format   Format stored inside GPU.
     * \param data_format       The format of the given data.
     * \param data_type         Format of the data.
     * \param data              Pointer to the data to upload.
     * \param size              Dimension size of the texture.
     * \param pixels_per_line   Number of pixels per row. Use 0 for default.
     *
     * \returns Handle to the texture.
     */
    drivers::handle create_texture(graphics_driver *driver, const std::uint8_t dim, const std::uint8_t mip_levels,
        drivers::texture_format internal_format, drivers::texture_format data_format, drivers::texture_data_type data_type,
        const void *data, const std::size_t total_data_size, const eka2l1::vec3 &size, const std::size_t pixels_per_line = 0,
        const std::uint32_t unpack_alignment = 4);

    /**
     * @brief Create a new renderbuffer.
     * 
     * This is a surface that does not allow direct upload from the client, used for framebuffer. Depends on implementation,
     * this may simply be a texture.
     * 
     * @param driver                The driver associated with this renderbuffer.
     * @param size                  The dimension of the renderbuffer.
     * @param internal_format       The internal format used to store the surface data.
     * 
     * @returns Handle to the renderbuffer. 0 on failure.
     */
    drivers::handle create_renderbuffer(graphics_driver *driver, const eka2l1::vec2 &size, const drivers::texture_format internal_format);

    /**
     * \brief Create a new buffer.
     *
     * \param driver           The driver associated with the buffer.
     * \param initial_data     The initial data to supply to the buffer.
     * \param initial_size     The size of the buffer. Later resize won't keep the buffer data.
     * \param upload_hint      Upload frequency and target hint for the buffer.
     *
     * \returns Handle to the buffer.
     */
    drivers::handle create_buffer(graphics_driver *driver, const void *initial_data, const std::size_t initial_size, const buffer_upload_hint upload_hint);

    /**
     * @brief Create input layout descriptors for input vertex data.
     * 
     * @brief driver            The driver associated with the input descriptor.
     * @brief descriptors       The layout infos.
     * @brief count             Number of descriptor provided.
     */
    drivers::handle create_input_descriptors(graphics_driver *driver, input_descriptor *descriptors, const std::uint32_t count);

    /**
     * @brief Create a framebuffer object.
     * 
     * If the framebuffer to use DEPTH24_STENCIL8, let the depth and stencil buffer point to the same handle.
     * 
     * @param driver                 The driver associated with the framebuffer.
     * @param color_buffers          An array containing list of color buffers that will be attached to this framebuffer.
     * @param color_face_indicies    An array specify the index of the face (in case the drawable is a cube map) of the texture to render to.
     * @param color_buffer_count     The number of color buffer in the array upper.
     * @param depth_buffer           Handle to the depth buffer (texture/renderbuffer).
     * @param depth_face_index       The index of the face in texture to render the depth buffer to.
     * @param stencil_buffer         Handle to the stencil buffer (texture/renderbuffer).
     * @param stencil_face_index       The index of the face in texture to render the depth buffer to.
     * 
     * @return Non-zero handle on success.
     */
    drivers::handle create_framebuffer(graphics_driver *driver, const drivers::handle *color_buffers, const int *color_face_indicies,
        const std::uint32_t color_buffer_count, drivers::handle depth_buffer, const int depth_face_index,
        drivers::handle stencil_buffer, const int stencil_face_index);

    /**
     * @brief Read bitmap data from a region into memory buffer.
     * 
     * The data will also be word-aligned each line pitch.
     * 
     * @param h             Handle to the bitmap.
     * @param pos           The position to start clipping bitmap data from.
     * @param size          The size of the clipped bitmap region.
     * @param bpp           The target BPP that will be written to the memory.
     * @param buffer_ptr    The buffer to read the data into.
     * @param buffer_size   Size of the buffer to read the data to.
     * 
     * @returns False on error (either the buffer size is inefficient or some errors happen during the read).
     */
    bool read_bitmap(graphics_driver *driver, drivers::handle h, const eka2l1::point &pos, const eka2l1::object_size &size,
        const std::uint32_t bpp, std::uint8_t *buffer_ptr);

    static constexpr std::size_t MAX_THRESHOLD_TO_FLUSH = 12000;
    static constexpr std::size_t MAX_CAP_COMMAND_COUNT = 12800;

    #define PACK_2U32_TO_U64(a, b) (static_cast<std::uint64_t>(b) << 32) | static_cast<std::uint32_t>(a)

    template <typename T, typename R>
    void unpack_u64_to_2u32(const std::uint64_t source, T &a, R &b) {
        a = static_cast<T>(source & 0xFFFFFFFF);
        b = static_cast<R>(source >> 32);
    }

    std::uint64_t pack_from_two_floats(const float f1, const float f2);
    void unpack_to_two_floats(const std::uint64_t source, float &f1, float &f2);

    class graphics_command_builder {
    protected:
        command_list list_;

    public:
        explicit graphics_command_builder()
            : list_(MAX_CAP_COMMAND_COUNT) {
        }

        ~graphics_command_builder() {
            if (list_.base_) {
                delete list_.base_;
            }
        }

        bool is_empty() const {
            return list_.size_ == 0;
        }

        bool need_flush() const {
            return (list_.size_ >= MAX_THRESHOLD_TO_FLUSH);
        }

        void reset_list() {
            delete list_.base_;

            list_.size_ = 0;
            list_.base_ = nullptr;
        }

        command_list retrieve_command_list() {
            command_list copy = list_;
            list_.base_ = nullptr;
            list_.size_ = 0;

            return copy;
        }

        command *create_next_command() {
            return list_.retrieve_next();
        }

        bool merge(command_list &another) {
            if (!another.base_ || !another.size_) {
                return true;
            }

            if (list_.base_ == nullptr) {
                list_ = another;
            } else {
                if (another.size_ + list_.size_ > list_.max_cap_) {
                    return false;
                }

                std::memcpy(list_.base_ + list_.size_, another.base_, another.size_ * sizeof(command));
                list_.size_ += another.size_;
            }

            delete[] another.base_;
            return true;
        }

        void set_brush_color_detail(const eka2l1::vec4 &color);

        void set_brush_color(const eka2l1::vec3 &color) {
            set_brush_color_detail({ color.x, color.y, color.z, 255 });
        }

        /**
         * \brief Set scissor rectangle, allow redraw only in specified area if clipping is enabled.
         *
         * Use in drawing window rect or invalidate a specific region of an window.
         * 
         * If the height is negative, the clip will use left-hand coordinates.
         */
        void clip_rect(const eka2l1::rect &rect);
        void clip_bitmap_rect(const eka2l1::rect &rect);

        void clip_bitmap_region(const common::region &region, const float scale_factor = 1.0f);

        /**
          * \brief Clear the binding bitmap with color.
          * \params clear_parameters The first four float is color buffer value, the next two floats are depth and stencil.
          */
        void clear(vecx<float, 6> clear_parameters, const std::uint8_t clear_bitarr);

        /**
         * \brief Set a bitmap to be current read/draw buffer.
         *
         * Binding a bitmap results in draw operations being done within itself.
         *
         * \param handle Handle to the bitmap.
         */
        void bind_bitmap(const drivers::handle handle);

        void bind_bitmap(const drivers::handle draw_handle, const drivers::handle read_handle);

        void resize_bitmap(drivers::handle h, const eka2l1::vec2 &new_size);

        /**
         * \brief Update a bitmap' data region.
         *
         * The dimensions and the offset parameter must reside in bitmap boundaries create before.
         * The behavior will be undefined if these conditions are not satisfied. Though some infos will be emitted.
         *
         * \param h                 The handle to existing bitmap.
         * \param data              Pointer to bitmap data.
         * \param size              Size of bitmap data.
         * \param offset            The offset of the bitmap (pixels).
         * \param dim               The dimensions of bitmap (pixels).
         * \param pixels_per_line   Number of pixels per row. Use 0 for default.
         * 
         * \returns Handle to the texture.
         */
        void update_bitmap(drivers::handle h, const char *data, const std::size_t size,
            const eka2l1::vec2 &offset, const eka2l1::vec2 &dim, const std::size_t pixels_per_line = 0,
            const bool need_copy = true);

        /**
         * @brief Update a texture data region.
         * 
         * It's recommended to match data format and data type to the arguments passed in
         * texture creation.
         * 
         * @param h                 The handle to existing bitmap.
         * @param data              Pointer to bitmap data.
         * @param size              Size of bitmap data.
         * @param data_format       The layout format of the data.
         * @param data_type         Type of each pixel of the data.
         * @param offset            The offset of the bitmap (pixels).
         * @param dim               The dimensions of bitmap (pixels).
         * @param pixels_per_line   Number of pixels per row. Use 0 for default.
         * 
         * @returns Handle to the texture.
         */
        void update_texture(drivers::handle h, const char *data, const std::size_t size,
            const std::uint8_t level, const texture_format data_format, const texture_data_type data_type,
            const eka2l1::vec3 &offset, const eka2l1::vec3 &dim, const std::size_t pixels_per_line = 0,
            const std::uint32_t unpack_alignment = 4);

        /**
         * \brief Draw a bitmap to currently binded bitmap.
         *
         * The only limitation is origin is hard-coded to the center of the bitmap for rotation purpose.
         * 
         * \param h            The handle of the bitmap to blit.
         * \param maskh        The handle of the mask to apply to this bitmap. Use 0 for none.
         * \param dest_rect    The destination rectangle of the bitmap on the screen.
         * \param source_rect  The source rectangle to strip.
         * \param origin       The origin to which the rotation is based on.
         * \param rotation     The rotation in degrees.
         * \param flags        Drawing flags.
         */
        void draw_bitmap(drivers::handle h, drivers::handle maskh, const eka2l1::rect &dest_rect,
            const eka2l1::rect &source_rect, const eka2l1::vec2 &origin = eka2l1::vec2(0, 0),
            const float rotation = 0.0f, const std::uint32_t flags = 0);

        /**
         * \brief Draw a rectangle with brush color.
         * 
         * \param target_rect The destination rectangle.
         */
        void draw_rectangle(const eka2l1::rect &target_rect);

        /**
         * \brief Use a shader program.
         */
        void use_program(drivers::handle h);

        /**
         * @brief Set the standalone uniform value for the current active program.
         * 
         * These uniforms are declared globally, and do not belong in any buffer objects in the linked program.
         * Note that for now only a number of backends support this (OpenGL and OpenGLES). Preferred option
         * is to store uniform data inside a buffer block.
         * 
         * Backend that does not support this will either ignore or additionally prints out some warning log
         * in the console. Some backends may emulate this. A shader program may support dynamic uniforms if
         * metadata is attached.
         * 
         * @param binding       The identifier of the variable in the linked program.
         * @param var_type      The variable type for uploading data.
         * @param data          The data to set to the uniform variable.
         * @param data_size     The size of the data buffer in bytes.
         */
        void set_dynamic_uniform(const int binding, const drivers::shader_var_type var_type,
            const void *data, const std::size_t data_size);

        /**
         * \brief Bind a texture or bitmap (as texture) to a binding slot.
         *
         * \param h       Handle to the texture.
         * \param binding Number of slot to bind.
         */
        void bind_texture(drivers::handle h, const int binding);

        /**
         * @brief Set which texture in a slot that the specified active shader module will use.
         * 
         * Note for some backends, the binding might be shared between programs, making the module specification
         * useless.
         * 
         * @param texture_slot          Texture slot to bind to shader.
         * @param shader_binding        The target binding of the shader variable.
         * @param module                The module that the texture variable is active on.
         */
        void set_texture_for_shader(const int texture_slot, const int shader_binding,
            const drivers::shader_module_type module);

        /**
         * \brief Set vertex buffers to their slots
         *
         * \param h                     Handle to the array of buffer handle.
         * \param starting_slots        The starting slot index to set these buffers.
         * \param count                 The number of buffers to set.
         */
        void set_vertex_buffers(drivers::handle *h, const std::uint32_t starting_slots, const std::uint32_t count);

        void set_index_buffer(drivers::handle h);

        /**
         * @brief Draw array of vertices.
         * 
         * @param prim_mode         The primitive mode used to draw the vertices data.
         * @param first             The starting index of the vertices data.
         * @param count             Number of vertices to be drawn.
         * @param instance_count    Number of instance.
         */
        void draw_arrays(const graphics_primitive_mode prim_mode, const std::int32_t first,
            const std::int32_t count, const std::int32_t instance_count);

        /**
         * \brief Draw indexed vertices.
         *
         * \param prim_mode   The primitive mode used to draw the vertices data.
         * \param count       Number of vertices to be drawn.
         * \param index_type  Index variable format of the index buffer data.
         * \param index_off   Offset to beginning taking the index data from.
         * \param vert_base   Offset to beginning taking the vertex data from.
         */
        void draw_indexed(const graphics_primitive_mode prim_mode, const int count, const data_format index_type, const int index_off, const int vert_base);

        /**
         * \brief Update buffer data, with provided chunks.
         *
         * All chunks are merged into one single buffer and the target buffer will be updated.
         *
         * \param h                 The handle to the buffer.
         * \param offset            Offset to start writing buffer to.
         * \param chunk_count       Total number of chunk.
         * \param chunk_ptr         Pointer to all chunks.
         * \param chunk_size        Pointer to size of all chunks.
         */
        void update_buffer_data(drivers::handle h, const std::size_t offset, const int chunk_count, const void **chunk_ptr, const std::uint32_t *chunk_size);

        /**
         * \brief Set current viewport.
         * 
         * \param viewport_rect     The rectangle to set viewport to
         */
        void set_viewport(const eka2l1::rect &viewport_rect);
        void set_bitmap_viewport(const eka2l1::rect &viewport_rect);

        /**
         * \brief Enable/disable a graphic feature.
         */
        void set_feature(drivers::graphics_feature feature, const bool enabled);

        /**
         * \brief Fill a blend formula.
         *
         * Color that will be written to buffer will be calculated using the following formulas:
         *   dest.rgb = rgb_blend_equation(frag_out.rgb * rgb_frag_out_factor + current.rgb * rgb_current_factor)
         *   dest.a = a_blend_equation(frag_out.a * a_frag_out_factor + a.rgb * a_current_factor)
         * 
         * \param rgb_equation              Equation for calculating blended RGB.
         * \param a_equation                Equation for calculating blended Alpha.
         * \param rgb_frag_output_factor    The factor with RGB fragment output.
         * \param rgb_current_factor        The factor with current pixel's RGB inside color buffer.
         * \param a_frag_output_factor      The factor with alpha fragment output.
         * \param a_current_factor          The factor with current pixel's alpha inside color buffer.
         */
        void blend_formula(const blend_equation rgb_equation, const blend_equation a_equation,
            const blend_factor rgb_frag_output_factor, const blend_factor rgb_current_factor,
            const blend_factor a_frag_output_factor, const blend_factor a_current_factor);

        /**
         * @brief Set action to the write for a pixel to stencil buffer on circumstances.
         * 
         * @param face_operate_on                   The face to base the actions on.
         * @param on_stencil_fail                   Action to take on stencil test fails.
         * @param on_stencil_pass_depth_fail        Action to take on stencil test passes but depth test fails.
         * @param on_both_stencil_depth_pass        Action to take when both stencil and depth test pass.
         */
        void set_stencil_action(const rendering_face face_operate_on, const stencil_action on_stencil_fail,
            const stencil_action on_stencil_pass_depth_fail, const stencil_action on_both_stencil_depth_pass);

        /**
         * @brief Set stencil pass condition.
         * 
         * @param face_operate_on           The face to set the pass condition on.
         * @param cond_func                 The function used to determine the pass.
         * @param cond_func_ref_value       The value argument to be used in the function.
         * @param mask                      The mask that is AND to both value in stencil buffer with the ref value.
         */
        void set_stencil_pass_condition(const rendering_face face_operate_on, const condition_func cond_func,
            const int cond_func_ref_value, const std::uint32_t mask);

        /**
         * @brief Set the value to AND with each value be written to stencil buffer.
         * 
         * @param face_operate_on       The face to set this mask to
         * @param mask                  The mask to set.
         */
        void set_stencil_mask(const rendering_face face_operate_on, const std::uint32_t mask);

        /**
         * @brief Set the value to AND with each value that will be written to depth buffer.
         *
         * @param mask                  The mask to set.
         */
        void set_depth_mask(const std::uint32_t mask);

        void set_depth_pass_condition(const condition_func func);

        /**
         * \brief Save state to temporary storage.
         */
        void backup_state();

        /**
         * \brief Load state from previously saved temporary storage.
         */
        void load_backup_state();

        /**
         * \brief Present swapchain to screen.
         */
        void present(int *status);

        /**
         * \brief Destroy an object.
         *
         * \param h Handle to the object.
         */
        void destroy(drivers::handle h);

        void destroy_bitmap(drivers::handle h);

        void set_swapchain_size(const eka2l1::vec2 &swsize);

        /**
         * @brief Set the size of the display in ortho matrix.
         * 
         * By default the size of swapchain is used for the ortho matrix, everytime the swapchain size
         * is set. This overrides that.
         * 
         * @param osize     Size to set in the ortho matrix.
         */
        void set_ortho_size(const eka2l1::vec2 &osize);

        // TODO: Document
        void set_texture_filter(drivers::handle h, const bool is_min, const drivers::filter_option mag);

        void set_texture_addressing_mode(drivers::handle h, const drivers::addressing_direction dir, const drivers::addressing_option opt);

        void set_texture_anisotrophy(drivers::handle h, const float anisotrophy_fact);

        void set_texture_max_mip(drivers::handle h, const std::uint32_t max_mip);

        void regenerate_mips(drivers::handle h);

        /**
         * \brief Set channel swizzlings of an image.
         * 
         * \param h Handle to the object (texture/bitmap).
         * 
         * \param r Swizzling of first channel.
         * \param g Swizzling of second channel.
         * \param b Swizzling of third channel.
         * \param a Swizzling of fourth channel.
         */
        void set_swizzle(drivers::handle h, drivers::channel_swizzle r, drivers::channel_swizzle g, drivers::channel_swizzle b,
            drivers::channel_swizzle a);

        /**
         * @brief Set the size of line drawn.
         * 
         * @param value     New point size.
         */
        void set_point_size(const std::uint8_t value);

        /**
         * @brief Set the style of lines that drawn by draw line or polygon function.
         * @param style     New line style to set. Width and spacing of pattern are fixed for each style.
         */
        void set_pen_style(const pen_style style);

        /**
         * @brief Draw a line.
         * 
         * If you want to draw multiple lines that are connected together, consider using draw_polygon.
         * 
         * This function is optimized for drawing a single line, so it's also advised to use this function
         * when draw_polygon only contains two points.
         * 
         * @param start The starting position of the line in screen coordinate.
         * @param end The end position of the line in screen coordinate.
         */
        void draw_line(const eka2l1::point &start, const eka2l1::point &end);

        /**
         * @brief Draw a polygon.
         * 
         * If the polygon point count is 2, consider using draw_line.
         * 
         * @param point_list    Pointer to the list of point belongs to the polygon
         * @param point_count   The number of points in the polygon, must be >= 2, else behaviour is relied on the backend.
         */
        void draw_polygons(const eka2l1::point *point_list, const std::size_t point_count);

        /**
         * @brief Set the face to be culled.
         * 
         * Default face to be culled depends on the backend, which usually is back. In addition, culling is off by default, and
         * only applies when it's enabled through set_cull_mode.
         * 
         * @param face The face to be culled.
         */
        void set_cull_face(const rendering_face face);

        /**
         * @brief Set the rule of vertices direction to determine which face is in the front and which face is the back.
         * 
         * @param rule The rule to set.
         */
        void set_front_face_rule(const rendering_face_determine_rule rule);

        /**
         * @brief Recreate an existing texture.
         * 
         * No failure is reported to the client.
         * 
         * @param h                 Handle to the existing texture.
         * @param dim               Total dimensions of this texture.
         * @param mip_levels        Total mips that the data contains.
         * @param internal_format   Format stored inside GPU.
         * @param data_format       The format of the given data.
         * @param data_type         Format of the data.
         * @param data              Pointer to the data to upload.
         * @param size              Dimension size of the texture.
         * @param data_size         Data size.
         * @param pixels_per_line   Number of pixels per row. Use 0 for default.
         */
        void recreate_texture(drivers::handle h, const std::uint8_t dim, const std::uint8_t mip_levels,
            drivers::texture_format internal_format, drivers::texture_format data_format, drivers::texture_data_type data_type,
            const void *data, const std::size_t data_size, const eka2l1::vec3 &size, const std::size_t pixels_per_line = 0,
            const std::uint32_t unpack_alignment = 4);

        /**
         * @brief Recreate an existing buffer. 
         * 
         * @param h                 Handle to an existing buffer.
         * @param initial_data      The initial data of the buffer. This is copied in this function.
         * @param initial_size      The size of the initial data.
         * @param hint              A hint on the buffer's intended usage.
         * @param upload_hint       A hint on the buffer's upload behaviour.
         */
        void recreate_buffer(drivers::handle h, const void *initial_data, const std::size_t initial_size, const buffer_upload_hint upload_hint);

        /**
         * @brief Update the existing input descriptors.
         * 
         * @param h                 Handle to an existing input descriptors.
         * @brief descriptors       The layout infos.
         * @brief count             Number of descriptor provided.
         */
        void update_input_descriptors(drivers::handle h, input_descriptor *descriptors, const std::uint32_t count);

        void bind_input_descriptors(drivers::handle h);

        /**
         * @brief Set the mask for color outputted in the fragment shader stage.
         * 
         * @param mask 8-bit integer with the first 4 least significant bits, each bit set represnenting which color component can be written.
         */
        void set_color_mask(const std::uint8_t mask);

        void set_line_width(const float width);

        void set_depth_bias(float constant_factor, float clamp, float slope_factor);

        void set_depth_range(const float min, const float max);

        void recreate_renderbuffer(drivers::handle h, const eka2l1::vec2 &size, const drivers::texture_format internal_format);

        void set_framebuffer_color_buffer(drivers::handle h, drivers::handle color_buffer, const int face_index, const std::int32_t color_index = -1);
        void set_framebuffer_depth_stencil_buffer(drivers::handle h, drivers::handle depth, const int depth_face_index, drivers::handle stencil, const int stencil_face_index);
        void bind_framebuffer(drivers::handle h, drivers::framebuffer_bind_type bind_type);
    };

    void advance_draw_pos_around_origin(eka2l1::rect &origin_normal_rect, const int rotation);
}