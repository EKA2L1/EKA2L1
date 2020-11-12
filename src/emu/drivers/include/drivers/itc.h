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
#include <drivers/graphics/buffer.h>
#include <drivers/graphics/common.h>
#include <drivers/graphics/shader.h>
#include <drivers/graphics/texture.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace eka2l1::drivers {
    struct command_list;
    class graphics_driver;

    using graphics_driver_dialog_callback = std::function<void(const char*)>;

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

    /** \brief Create a new shader program.
      *
      * \param driver      The driver associated with the program. 
      * \param vert_data   Pointer to the vertex shader code.
      * \param vert_size   Size of vertex shader code.
      * \param frag_size   Size of fragment shader code.
      * \param metadata    Pointer to metadata struct. Some API may provide this.
      *
      * \returns Handle to the program.
      */
    drivers::handle create_program(graphics_driver *driver, const char *vert_data, const std::size_t vert_size,
        const char *frag_data, const std::size_t frag_size, shader_metadata *metadata = nullptr);

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
        const void *data, const eka2l1::vec3 &size, const std::size_t pixels_per_line = 0);

    /**
     * \brief Create a new buffer.
     *
     * \param driver           The driver associated with the buffer.
     * \param initial_size     The size of the buffer. Later resize won't keep the buffer data.
     * \param hint             Usage hint of the buffer.
     * \param upload_hint      Upload frequency and target hint for the buffer.
     *
     * \returns Handle to the buffer.
     */
    drivers::handle create_buffer(graphics_driver *driver, const std::size_t initial_size, const buffer_hint hint,
        const buffer_upload_hint upload_hint);

    /**
     * @brief   Open a native file/folder dialog.
     * 
     * @param   driver      The driver to request native dialog.
     * @param   filter      Extension filter string.
     * @param   callback    Function invoked when a folder/file is choosed.
     * @param   is_folder   If this is true, the dialog will be for a folder.
     * 
     * @returns False if this dialog is canceled.
     */
    bool open_native_dialog(graphics_driver *driver, const char *filter, drivers::graphics_driver_dialog_callback callback, const bool is_folder = false);

    struct graphics_command_list {
        virtual bool empty() const = 0;
    };

    struct server_graphics_command_list : public graphics_command_list {
        command_list list_;

        bool empty() const override {
            return list_.empty();
        }
    };

    class graphics_command_list_builder {
    protected:
        graphics_command_list *list_;

    public:
        explicit graphics_command_list_builder(graphics_command_list *list)
            : list_(list) {
        }

        virtual ~graphics_command_list_builder() {}

        virtual void set_brush_color_detail(const eka2l1::vecx<int, 4> &color) = 0;

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
        virtual void clip_rect(eka2l1::rect &rect) = 0;

        /**
         * \brief Enable/disable clipping (scissor).
         */
        virtual void set_clipping(const bool enabled) = 0;

        /**
          * \brief Clear the binding bitmap with color.
          * \params color A RGBA vector 4 color
          */
        virtual void clear(vecx<std::uint8_t, 4> color, const std::uint8_t clear_bitarr) = 0;

        /**
         * \brief Set a bitmap to be current.
         *
         * Binding a bitmap results in draw operations being done within itself.
         *
         * \param handle Handle to the bitmap.
         */
        virtual void bind_bitmap(const drivers::handle handle) = 0;

        virtual void resize_bitmap(drivers::handle h, const eka2l1::vec2 &new_size) = 0;

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
        virtual void update_bitmap(drivers::handle h, const char *data, const std::size_t size,
            const eka2l1::vec2 &offset, const eka2l1::vec2 &dim, const std::size_t pixels_per_line = 0)
            = 0;

        /**
         * \brief Draw a bitmap to currently binded bitmap.
         *
         * \param h            The handle of the bitmap to blit.
         * \param maskh        The handle of the mask to apply to this bitmap. Use 0 for none.
         * \param pos          The position of the bitmap on the screen.
         * \param source_rect  The source rectangle to strip.
         * \param flags        Drawing flags.
         */
        virtual void draw_bitmap(drivers::handle h, drivers::handle maskh, const eka2l1::rect &dest_rect, const eka2l1::rect &source_rect, const std::uint32_t flags = 0) = 0;

        /**
         * \brief Draw a rectangle with brush color.
         * 
         * \param target_rect The destination rectangle.
         */
        virtual void draw_rectangle(const eka2l1::rect &target_rect) = 0;

        /**
         * \brief Use a shader program.
         */
        virtual void use_program(drivers::handle h) = 0;

        virtual void set_uniform(drivers::handle h, const int binding, const drivers::shader_set_var_type var_type,
            const void *data, const std::size_t data_size)
            = 0;

        /**
         * \brief Bind a texture or bitmap (as texture) to a binding slot.
         *
         * \param h       Handle to the texture.
         * \param binding Number of slot to bind.
         */
        virtual void bind_texture(drivers::handle h, const int binding) = 0;

        /**
         * \brief Bind a buffer.
         *
         * \param h     Handle to the buffer.
         */
        virtual void bind_buffer(drivers::handle h) = 0;

        /**
         * \brief Draw indexed vertices.
         *
         * \param prim_mode   The primitive mode used to draw the vertices data.
         * \param count       Number of vertices to be drawn.
         * \param index_type  Index variable format of the index buffer data.
         * \param index_off   Offset to beginning taking the index data from.
         * \param vert_base   Offset to beginning taking the vertex data from.
         */
        virtual void draw_indexed(const graphics_primitive_mode prim_mode, const int count, const data_format index_type, const int index_off, const int vert_base) = 0;

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
        virtual void update_buffer_data(drivers::handle h, const std::size_t offset, const int chunk_count, const void **chunk_ptr, const std::uint32_t *chunk_size) = 0;

        /**
         * \brief Set current viewport.
         * 
         * \param viewport_rect     The rectangle to set viewport to
         */
        virtual void set_viewport(const eka2l1::rect &viewport_rect) = 0;

        /**
         * \brief Enable/disable depth test.
         *
         * \param enable True if the depth test is suppose to be enable.
         */
        virtual void set_depth(const bool enable) = 0;

        virtual void set_stencil(const bool enable) = 0;

        virtual void set_cull_mode(const bool enable) = 0;

        virtual void set_blend_mode(const bool enable) = 0;

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
        virtual void blend_formula(const blend_equation rgb_equation, const blend_equation a_equation,
            const blend_factor rgb_frag_output_factor, const blend_factor rgb_current_factor,
            const blend_factor a_frag_output_factor, const blend_factor a_current_factor)
            = 0;

        /**
         * @brief Set action to the write for a pixel to stencil buffer on circumstances.
         * 
         * @param face_operate_on                   The face to base the actions on.
         * @param on_stencil_fail                   Action to take on stencil test fails.
         * @param on_stencil_pass_depth_fail        Action to take on stencil test passes but depth test fails.
         * @param on_both_stencil_depth_pass        Action to take when both stencil and depth test pass.
         */
        virtual void set_stencil_action(const stencil_face face_operate_on, const stencil_action on_stencil_fail,
            const stencil_action on_stencil_pass_depth_fail, const stencil_action on_both_stencil_depth_pass) = 0;

        /**
         * @brief Set stencil pass condition.
         * 
         * @param face_operate_on           The face to set the pass condition on.
         * @param cond_func                 The function used to determine the pass.
         * @param cond_func_ref_value       The value argument to be used in the function.
         * @param mask                      The mask that is AND to both value in stencil buffer with the ref value.
         */
        virtual void set_stencil_pass_condition(const stencil_face face_operate_on, const condition_func cond_func,
            const int cond_func_ref_value, const std::uint32_t mask) = 0;

        /**
         * @brief Set the value to AND with each value be written to stencil buffer.
         * 
         * @param face_operate_on       The face to set this mask to
         * @param mask                  The mask to set.
         */
        virtual void set_stencil_mask(const stencil_face face_operate_on, const std::uint32_t mask) = 0;

        /**
         * \brief Save state to temporary storage.
         */
        virtual void backup_state() = 0;

        /**
         * \brief Load state from previously saved temporary storage.
         */
        virtual void load_backup_state() = 0;

        /**
         * \brief Attach descriptors and its layout to a buffer that is used as vertex.
         *
         * \param h                 The handle to the buffer.
         * \brief stride            The total of bytes a vertex/instance consists of.
         * \brief instance_move     Add the vertex cursor with the stride for each instance if true.
         * \param descriptors       Pointer to the descriptors.
         *                          This is copied and doesn't need to exist after calling this function.
         * \param descriptor_count  The number of descriptors that we want to be attached.
         */
        virtual void attach_descriptors(drivers::handle h, const int stride, const bool instance_move,
            const attribute_descriptor *descriptors, const int descriptor_count)
            = 0;

        /**
         * \brief Present swapchain to screen.
         */
        virtual void present(int *status) = 0;

        /**
         * \brief Destroy an object.
         *
         * \param h Handle to the object.
         */
        virtual void destroy(drivers::handle h) = 0;

        virtual void destroy_bitmap(drivers::handle h) = 0;

        virtual void set_swapchain_size(const eka2l1::vec2 &swsize) = 0;

        // TODO: Document
        virtual void set_texture_filter(drivers::handle h, const drivers::filter_option min, const drivers::filter_option mag) = 0;

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
        virtual void set_swizzle(drivers::handle h, drivers::channel_swizzle r, drivers::channel_swizzle g, drivers::channel_swizzle b,
            drivers::channel_swizzle a)
            = 0;
    };

    class server_graphics_command_list_builder : public graphics_command_list_builder {
        command_list &get_command_list() {
            return reinterpret_cast<server_graphics_command_list *>(list_)->list_;
        }

        void create_single_set_command(const std::uint16_t op, const bool enable);

    public:
        explicit server_graphics_command_list_builder(graphics_command_list *list);

        void set_brush_color_detail(const eka2l1::vecx<int, 4> &color) override;

        /**
         * \brief Set scissor rectangle, allow redraw only in specified area if clipping is enabled.
         *
         * Use in drawing window rect or invalidate a specific region of an window.
         */
        void clip_rect(eka2l1::rect &rect) override;

        /**
         * \brief Enable/disable clipping (scissor).
         */
        void set_clipping(const bool enabled) override;

        /**
          * \brief Clear the binding bitmap with color.
          * \params color A RGBA vector 4 color
          */
        void clear(vecx<std::uint8_t, 4> color, const std::uint8_t clear_bitarr) override;

        /**
         * \brief Set a bitmap to be current.
         *
         * Binding a bitmap results in draw operations being done within itself.
         *
         * \param handle Handle to the bitmap.
         */
        void bind_bitmap(const drivers::handle handle) override;

        void resize_bitmap(drivers::handle h, const eka2l1::vec2 &new_size) override;

        void update_bitmap(drivers::handle h, const char *data, const std::size_t size, const eka2l1::vec2 &offset,
            const eka2l1::vec2 &dim, const std::size_t pixels_per_line = 0) override;

        void draw_bitmap(drivers::handle h, drivers::handle maskh, const eka2l1::rect &dest_rect, const eka2l1::rect &source_rect, const std::uint32_t flags = 0) override;

        void draw_rectangle(const eka2l1::rect &target_rect) override;

        void use_program(drivers::handle h) override;

        void set_uniform(drivers::handle h, const int binding, const drivers::shader_set_var_type var_type,
            const void *data, const std::size_t data_size) override;

        void bind_texture(drivers::handle h, const int binding) override;

        void bind_buffer(drivers::handle h) override;

        void update_buffer_data(drivers::handle h, const std::size_t offset, const int chunk_count, const void **chunk_ptr, const std::uint32_t *chunk_size) override;

        void draw_indexed(const graphics_primitive_mode prim_mode, const int count, const data_format index_type, const int index_off, const int vert_base) override;

        void set_viewport(const eka2l1::rect &viewport_rect) override;

        void set_depth(const bool enable) override;
        
        void set_stencil(const bool enable) override;

        void set_cull_mode(const bool enable) override;

        void set_blend_mode(const bool enable) override;

        void blend_formula(const blend_equation rgb_equation, const blend_equation a_equation,
            const blend_factor rgb_frag_output_factor, const blend_factor rgb_current_factor,
            const blend_factor a_frag_output_factor, const blend_factor a_current_factor) override;

        void set_stencil_action(const stencil_face face_operate_on, const stencil_action on_stencil_fail,
            const stencil_action on_stencil_pass_depth_fail, const stencil_action on_both_stencil_depth_pass) override;
        
        void set_stencil_pass_condition(const stencil_face face_operate_on, const condition_func cond_func,
            const int cond_func_ref_value, const std::uint32_t mask) override;

        void set_stencil_mask(const stencil_face face_operate_on, const std::uint32_t mask) override;

        void attach_descriptors(drivers::handle h, const int stride, const bool instance_move,
            const attribute_descriptor *descriptors, const int descriptor_count) override;

        void backup_state() override;

        void load_backup_state() override;

        void present(int *status) override;

        void destroy(drivers::handle h) override;

        void destroy_bitmap(drivers::handle h) override;

        void set_swapchain_size(const eka2l1::vec2 &swsize) override;

        void set_texture_filter(drivers::handle h, const drivers::filter_option min, const drivers::filter_option mag) override;

        void set_swizzle(drivers::handle h, drivers::channel_swizzle r, drivers::channel_swizzle g, drivers::channel_swizzle b,
            drivers::channel_swizzle a) override;
    };

    struct graphics_command_callback_data {
        drivers::graphics_command_list_builder *builder_;
        void *userdata_;
    };
}