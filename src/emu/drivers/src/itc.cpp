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

#include <drivers/driver.h>
#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>

#include <chrono>
#include <cstring>

using namespace std::chrono_literals;

namespace eka2l1::drivers {
    static int send_sync_command(graphics_driver *drv, command cmd) {
        int status = -100;
        cmd.status_ = &status;

        command_list cmd_list(1);
        cmd_list.renew();

        cmd_list.size_ = 1;
        *cmd_list.base_ = cmd;

        std::unique_lock<std::mutex> ulock(drv->mut_);
        drv->submit_command_list(cmd_list);
        drv->cond_.wait(ulock, [&]() { return status != -100; });

        return status;
    }

    static std::uint64_t make_data_copy(const void *source, const std::size_t size) {
        if (!source) {
            return 0;
        }

        std::uint8_t *copy = new std::uint8_t[size];
        std::copy(reinterpret_cast<const std::uint8_t *>(source), reinterpret_cast<const std::uint8_t *>(source) + size, copy);

        return reinterpret_cast<std::uint64_t>(copy);
    }

    drivers::handle create_bitmap(graphics_driver *driver, const eka2l1::vec2 &size, const std::uint32_t bpp) {
        drivers::handle handle_num = 0;

        command cmd;
        cmd.opcode_ = graphics_driver_create_bitmap;
        cmd.data_[0] = PACK_2U32_TO_U64(size.x, size.y);
        cmd.data_[1] = bpp;
        cmd.data_[2] = reinterpret_cast<std::uint64_t>(&handle_num);

        if (send_sync_command(driver, cmd) != 0) {
            return 0;
        }

        return handle_num;
    }
    
    drivers::handle create_shader_module(graphics_driver *driver, const char *data, const std::size_t size, const shader_module_type mtype, std::string *compile_log) {
        drivers::handle handle_num = 0;

        command cmd;
        cmd.opcode_ = graphics_driver_create_shader_module;
        cmd.data_[0] = reinterpret_cast<std::uint64_t>(data);
        cmd.data_[1] = size;
        cmd.data_[2] = static_cast<std::uint64_t>(mtype);
        cmd.data_[3] = reinterpret_cast<std::uint64_t>(&handle_num);
        cmd.data_[4] = reinterpret_cast<std::uint64_t>(compile_log);

        if (send_sync_command(driver, cmd) != 0) {
            return 0;
        }

        return handle_num;
    }

    drivers::handle create_shader_program(graphics_driver *driver, drivers::handle vert_mod, drivers::handle frag_mod, shader_program_metadata *metadata, std::string *link_log) {
        drivers::handle handle_num = 0;
        std::uint8_t *metadata_ptr = nullptr;

        command cmd;
        cmd.opcode_ = graphics_driver_create_shader_program;
        cmd.data_[0] = vert_mod;
        cmd.data_[1] = frag_mod;
        cmd.data_[2] = reinterpret_cast<std::uint64_t>(&metadata_ptr);
        cmd.data_[3] = reinterpret_cast<std::uint64_t>(&handle_num);
        cmd.data_[4] = reinterpret_cast<std::uint64_t>(link_log);

        if (send_sync_command(driver, cmd) != 0) {
            return 0;
        }

        if (metadata_ptr && metadata)
            metadata->metadata_ = metadata_ptr;

        return handle_num;
    }

    drivers::handle create_buffer(graphics_driver *driver, const void *initial_data, const std::size_t initial_size, const buffer_upload_hint upload_hint) {
        drivers::handle handle_num = 0;

        command cmd;
        cmd.opcode_ = graphics_driver_create_buffer;
        cmd.data_[0] = reinterpret_cast<std::uint64_t>(initial_data);
        cmd.data_[1] = initial_size;
        cmd.data_[2] = static_cast<std::uint64_t>(upload_hint);
        cmd.data_[3] = 0;
        cmd.data_[4] = reinterpret_cast<std::uint64_t>(&handle_num);

        if (send_sync_command(driver, cmd) != 0) {
            return 0;
        }

        return handle_num;
    }
    
    drivers::handle create_input_descriptors(graphics_driver *driver, input_descriptor *descriptors, const std::uint32_t count) {
        drivers::handle handle_num = 0;

        command cmd;
        cmd.opcode_ = graphics_driver_create_input_descriptor;
        cmd.data_[0] = reinterpret_cast<std::uint64_t>(descriptors);
        cmd.data_[1] = count;
        cmd.data_[2] = 0;
        cmd.data_[3] = reinterpret_cast<std::uint64_t>(&handle_num);

        if (send_sync_command(driver, cmd) != 0) {
            return 0;
        }

        return handle_num;
    }

    drivers::handle create_texture(graphics_driver *driver, const std::uint8_t dim, const std::uint8_t mip_levels,
        drivers::texture_format internal_format, drivers::texture_format data_format, drivers::texture_data_type data_type,
        const void *data, const std::size_t data_size, const eka2l1::vec3 &size, const std::size_t pixels_per_line,
        const std::uint32_t unpack_alignment) {
        drivers::handle handle_num = 0;

        command cmd;

        cmd.opcode_ = graphics_driver_create_texture;
        cmd.data_[0] = dim | (static_cast<std::uint64_t>(mip_levels) << 8) | (static_cast<std::uint64_t>(internal_format) << 16)
            | (static_cast<std::uint64_t>(data_format) << 32) | (static_cast<std::uint64_t>(data_type) << 48);
        cmd.data_[1] = reinterpret_cast<std::uint64_t>(data);
        cmd.data_[2] = data_size;
        cmd.data_[3] = pixels_per_line;
        cmd.data_[4] = static_cast<std::uint64_t>(unpack_alignment);
        cmd.data_[5] = PACK_2U32_TO_U64(size.x, size.y);
        cmd.data_[6] = PACK_2U32_TO_U64(size.z, 0);
        cmd.data_[7] = 0;
        cmd.data_[8] = reinterpret_cast<std::uint64_t>(&handle_num);

        if (send_sync_command(driver, cmd) != 0) {
            return 0;
        }

        return handle_num;
    }
    
    drivers::handle create_renderbuffer(graphics_driver *driver, const eka2l1::vec2 &size, const drivers::texture_format internal_format) {
        drivers::handle handle_num = 0;
        command cmd;

        cmd.opcode_ = graphics_driver_create_renderbuffer;
        cmd.data_[0] = PACK_2U32_TO_U64(size.x, size.y);
        cmd.data_[1] = static_cast<std::uint64_t>(internal_format);
        cmd.data_[2] = 0;
        cmd.data_[3] = reinterpret_cast<std::uint64_t>(&handle_num);

        if (send_sync_command(driver, cmd) != 0) {
            return 0;
        }

        return handle_num;
    }
    
    drivers::handle create_framebuffer(graphics_driver *driver, const drivers::handle *color_buffers, const int *color_face_indicies,
        const std::uint32_t color_buffer_count, drivers::handle depth_buffer, const int depth_face_index,
        drivers::handle stencil_buffer, const int stencil_face_index) {
        drivers::handle handle_num = 0;
        command cmd;

        cmd.opcode_ = graphcis_driver_create_framebuffer;
        cmd.data_[0] = reinterpret_cast<std::uint64_t>(color_buffers);
        cmd.data_[1] = reinterpret_cast<std::uint64_t>(color_face_indicies);
        cmd.data_[2] = static_cast<std::uint64_t>(color_buffer_count);
        cmd.data_[3] = depth_buffer;
        cmd.data_[4] = stencil_buffer;
        cmd.data_[5] = PACK_2U32_TO_U64(depth_face_index, stencil_face_index);
        cmd.data_[6] = reinterpret_cast<std::uint64_t>(&handle_num);

        if (send_sync_command(driver, cmd) != 0) {
            return 0;
        }

        return handle_num;
    }
    
    bool read_bitmap(graphics_driver *driver, drivers::handle h, const eka2l1::point &pos, const eka2l1::object_size &size,
        const std::uint32_t bpp, std::uint8_t *buffer_ptr) {
        command cmd;

        cmd.opcode_ = graphics_driver_read_bitmap;
        cmd.data_[0] = h;
        cmd.data_[1] = PACK_2U32_TO_U64(pos.x, pos.y);
        cmd.data_[2] = PACK_2U32_TO_U64(size.x, size.y);
        cmd.data_[3] = bpp;
        cmd.data_[4] = reinterpret_cast<std::uint64_t>(buffer_ptr);

        return send_sync_command(driver, cmd);
    }

    std::uint64_t pack_from_two_floats(const float f1, const float f2) {
        return ((static_cast<std::uint64_t>(*reinterpret_cast<const std::uint32_t*>(&f2)) << 32) | *reinterpret_cast<const std::uint32_t*>(&f1));
    }

    void unpack_to_two_floats(const std::uint64_t source, float &f1, float &f2) {
        std::uint32_t high = static_cast<std::uint32_t>(source >> 32);
        std::uint32_t low = static_cast<std::uint32_t>(source );
        f1 = *reinterpret_cast<float*>(&low);
        f2 = *reinterpret_cast<float*>(&high);
    }

    void graphics_command_builder::clip_rect(const eka2l1::rect &rect) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_clip_rect;

        cmd->data_[0] = PACK_2U32_TO_U64(rect.top.x, rect.top.y);
        cmd->data_[1] = PACK_2U32_TO_U64(rect.size.x, rect.size.y);
    }

    void graphics_command_builder::clip_bitmap_rect(const eka2l1::rect &rect) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_clip_bitmap_rect;

        cmd->data_[0] = PACK_2U32_TO_U64(rect.top.x, rect.top.y);
        cmd->data_[1] = PACK_2U32_TO_U64(rect.size.x, rect.size.y);
    }

    void graphics_command_builder::clip_bitmap_region(const common::region &region, const float scale_factor) {
        if (region.rects_.size() == 0) {
            return;
        }

        if (region.rects_.size() == 1) {
            set_feature(graphics_feature::clipping, true);
            set_feature(graphics_feature::stencil_test, false);

            eka2l1::rect to_scale = region.rects_[0];
            to_scale.scale(scale_factor);

            clip_bitmap_rect(to_scale);
        } else {
            command *cmd = list_.retrieve_next();

            cmd->opcode_ = graphics_driver_clip_region;
            cmd->data_[0] = static_cast<std::uint64_t>(region.rects_.size());
            cmd->data_[1] = make_data_copy(region.rects_.data(), region.rects_.size() * sizeof(eka2l1::rect));
            cmd->data_[2] = pack_from_two_floats(scale_factor, 0.0f);
        }
    }

    void graphics_command_builder::clear(vecx<float, 6> color, const std::uint8_t clear_bitarr) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_clear;

        cmd->data_[0] = pack_from_two_floats(color[0], color[1]);
        cmd->data_[1] = pack_from_two_floats(color[2], color[3]);
        cmd->data_[2] = pack_from_two_floats(color[4], color[5]);
        cmd->data_[3] = clear_bitarr;
    }

    void graphics_command_builder::resize_bitmap(drivers::handle h, const eka2l1::vec2 &new_size) {
        // This opcode has two variant: sync or async.
        // The first argument is bitmap handle. If it's null then the currently binded one will be used.
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_resize_bitmap;
        cmd->data_[0] = h;
        cmd->data_[1] = PACK_2U32_TO_U64(new_size.x, new_size.y);
    }

    void graphics_command_builder::update_bitmap(drivers::handle h, const char *data, const std::size_t size,
        const eka2l1::vec2 &offset, const eka2l1::vec2 &dim, const std::size_t pixels_per_line, const bool need_copy) {
        // Copy data
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_update_bitmap;

        cmd->data_[0] = h;
        cmd->data_[1] = (need_copy ? make_data_copy(data, size) : reinterpret_cast<std::uint64_t>(data));
        cmd->data_[2] = size;
        cmd->data_[3] = PACK_2U32_TO_U64(offset.x, offset.y);
        cmd->data_[4] = PACK_2U32_TO_U64(dim.x, dim.y);
        cmd->data_[5] = pixels_per_line;
    }

    void graphics_command_builder::update_texture(drivers::handle h, const char *data, const std::size_t size, const std::uint8_t lvl,
        const texture_format data_format, const texture_data_type data_type,
        const eka2l1::vec3 &offset, const eka2l1::vec3 &dim, const std::size_t pixels_per_line, const std::uint32_t unpack_alignment) {
        // Copy data
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_update_texture;

        cmd->data_[0] = h;
        cmd->data_[1] = make_data_copy(data, size);
        cmd->data_[2] = size;
        cmd->data_[3] = lvl | (static_cast<std::uint64_t>(data_format) << 8) | (static_cast<std::uint64_t>(data_type) << 24); 
        cmd->data_[4] = PACK_2U32_TO_U64(offset.x, offset.y);
        cmd->data_[5] = PACK_2U32_TO_U64(offset.z, dim.x);
        cmd->data_[6] = PACK_2U32_TO_U64(dim.y, dim.z);
        cmd->data_[7] = pixels_per_line;
        cmd->data_[8] = unpack_alignment;
    }

    void graphics_command_builder::draw_bitmap(drivers::handle h, drivers::handle maskh, const eka2l1::rect &dest_rect, const eka2l1::rect &source_rect, const eka2l1::vec2 &origin,
        const float rotation, const std::uint32_t flags) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_draw_bitmap;

        cmd->data_[0] = h;
        cmd->data_[1] = maskh;
        cmd->data_[2] = PACK_2U32_TO_U64(dest_rect.top.x, dest_rect.top.y);
        cmd->data_[3] = PACK_2U32_TO_U64(dest_rect.size.x, dest_rect.size.y);
        cmd->data_[4] = PACK_2U32_TO_U64(source_rect.top.x, source_rect.top.y);
        cmd->data_[5] = PACK_2U32_TO_U64(source_rect.size.x, source_rect.size.y);
        cmd->data_[6] = PACK_2U32_TO_U64(origin.x, origin.y);
        cmd->data_[7] = (static_cast<std::uint64_t>(flags) << 32) | *reinterpret_cast<const std::uint32_t*>(&rotation);
    }

    void graphics_command_builder::bind_bitmap(const drivers::handle h) {
        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_bind_bitmap;
        cmd->data_[0] = h;
        cmd->data_[1] = 0;
    }

    void graphics_command_builder::bind_bitmap(const drivers::handle draw_handle, const drivers::handle read_handle) {
        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_bind_bitmap;
        cmd->data_[0] = draw_handle;
        cmd->data_[1] = read_handle;
    }

    void graphics_command_builder::draw_rectangle(const eka2l1::rect &target_rect) {
        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_draw_rectangle;
        cmd->data_[0] = PACK_2U32_TO_U64(target_rect.top.x, target_rect.top.y);
        cmd->data_[1] = PACK_2U32_TO_U64(target_rect.size.x, target_rect.size.y);
    }

    void graphics_command_builder::set_brush_color_detail(const eka2l1::vec4 &color) {
        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_set_brush_color;
        cmd->data_[0] = PACK_2U32_TO_U64(color.x, color.y);
        cmd->data_[1] = PACK_2U32_TO_U64(color.z, color.w);
    }

    void graphics_command_builder::use_program(drivers::handle h) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_use_program;
        cmd->data_[0] = h;
    }

    void graphics_command_builder::set_dynamic_uniform(const int binding, const drivers::shader_var_type var_type,
        const void *data, const std::size_t data_size) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_uniform;

        cmd->data_[0] = PACK_2U32_TO_U64(binding, var_type);
        cmd->data_[1] = make_data_copy(data, data_size);
        cmd->data_[2] = data_size;
    }

    void graphics_command_builder::bind_texture(drivers::handle h, const int binding) {
        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_bind_texture;
        cmd->data_[0] = h;
        cmd->data_[1] = static_cast<std::uint64_t>(binding);
    }

    void graphics_command_builder::draw_indexed(const graphics_primitive_mode prim_mode, const int count, const data_format index_type, const int index_off, const int vert_base) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_draw_indexed;

        cmd->data_[0] = PACK_2U32_TO_U64(prim_mode, count);
        cmd->data_[1] = PACK_2U32_TO_U64(index_type, index_off);
        cmd->data_[2] = static_cast<std::uint64_t>(vert_base);
    }

    void graphics_command_builder::draw_arrays(const graphics_primitive_mode prim_mode, const std::int32_t first, const std::int32_t count, const std::int32_t instance_count) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_draw_array;

        cmd->data_[0] = PACK_2U32_TO_U64(prim_mode, first);
        cmd->data_[1] = PACK_2U32_TO_U64(count, instance_count);
    }

    void graphics_command_builder::set_vertex_buffers(drivers::handle *h, const std::uint32_t starting_slot, const std::uint32_t count) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_bind_vertex_buffers;

        cmd->data_[0] = make_data_copy(h, sizeof(drivers::handle) * count);
        cmd->data_[1] = PACK_2U32_TO_U64(starting_slot, count);
    }

    void graphics_command_builder::set_index_buffer(drivers::handle h) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_bind_index_buffer;
        cmd->data_[0] = h;
    }

    void graphics_command_builder::update_buffer_data(drivers::handle h, const std::size_t offset, const int chunk_count, const void **chunk_ptr, const std::uint32_t *chunk_size) {
        std::size_t total_chunk_size = 0;
        std::uint32_t cursor = 0;

        for (int i = 0; i < chunk_count; i++) {
            total_chunk_size += chunk_size[i];
        }

        std::uint8_t *data = new std::uint8_t[total_chunk_size];

        for (int i = 0; i < chunk_count; i++) {
            std::copy(reinterpret_cast<const std::uint8_t *>(chunk_ptr[i]), reinterpret_cast<const std::uint8_t *>(chunk_ptr[i]) + chunk_size[i], data + cursor);
            cursor += chunk_size[i];
        }

        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_update_buffer;
        cmd->data_[0] = h;
        cmd->data_[1] = reinterpret_cast<std::uint64_t>(data);
        cmd->data_[2] = offset;
        cmd->data_[3] = total_chunk_size;
    }

    void graphics_command_builder::set_viewport(const eka2l1::rect &viewport_rect) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_viewport;

        cmd->data_[0] = PACK_2U32_TO_U64(viewport_rect.top.x, viewport_rect.top.y);
        cmd->data_[1] = PACK_2U32_TO_U64(viewport_rect.size.x, viewport_rect.size.y);
    }

    void graphics_command_builder::set_bitmap_viewport(const eka2l1::rect &viewport_rect) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_bitmap_viewport;

        cmd->data_[0] = PACK_2U32_TO_U64(viewport_rect.top.x, viewport_rect.top.y);
        cmd->data_[1] = PACK_2U32_TO_U64(viewport_rect.size.x, viewport_rect.size.y);
    }

    void graphics_command_builder::set_feature(drivers::graphics_feature feature, const bool enable) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_feature;
        cmd->data_[0] = PACK_2U32_TO_U64(feature, enable);
    }

    void graphics_command_builder::blend_formula(const blend_equation rgb_equation, const blend_equation a_equation,
        const blend_factor rgb_frag_output_factor, const blend_factor rgb_current_factor,
        const blend_factor a_frag_output_factor, const blend_factor a_current_factor) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_blend_formula;

        cmd->data_[0] = PACK_2U32_TO_U64(rgb_equation, a_equation);
        cmd->data_[1] = PACK_2U32_TO_U64(rgb_frag_output_factor, rgb_current_factor);
        cmd->data_[2] = PACK_2U32_TO_U64(a_frag_output_factor, a_current_factor);
    }

    void graphics_command_builder::set_stencil_action(const rendering_face face_operate_on, const stencil_action on_stencil_fail,
        const stencil_action on_stencil_pass_depth_fail, const stencil_action on_both_stencil_depth_pass) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_stencil_set_action;

        cmd->data_[0] = PACK_2U32_TO_U64(face_operate_on, on_stencil_fail);
        cmd->data_[1] = PACK_2U32_TO_U64(on_stencil_pass_depth_fail, on_both_stencil_depth_pass);
    }

    void graphics_command_builder::set_stencil_pass_condition(const rendering_face face_operate_on, const condition_func cond_func,
        const int cond_func_ref_value, const std::uint32_t mask) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_stencil_pass_condition;

        cmd->data_[0] = PACK_2U32_TO_U64(face_operate_on, cond_func);
        cmd->data_[1] = PACK_2U32_TO_U64(cond_func_ref_value, mask);
    }

    void graphics_command_builder::set_stencil_mask(const rendering_face face_operate_on, const std::uint32_t mask) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_stencil_set_mask;
        cmd->data_[0] = PACK_2U32_TO_U64(face_operate_on, mask);
    }

    void graphics_command_builder::backup_state() {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_backup_state;
    }

    void graphics_command_builder::load_backup_state() {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_restore_state;
    }

    void graphics_command_builder::present(int *status) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_display;
        cmd->status_ = status;
    }

    void graphics_command_builder::destroy(drivers::handle h) {
        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_destroy_object;
        cmd->data_[0] = h;
    }

    void graphics_command_builder::destroy_bitmap(drivers::handle h) {
        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_destroy_bitmap;
        cmd->data_[0] = h;
    }

    void graphics_command_builder::set_texture_filter(drivers::handle h, const bool is_min, const drivers::filter_option mag) {
        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_set_texture_filter;
        cmd->data_[0] = h;
        cmd->data_[1] = PACK_2U32_TO_U64(is_min, mag);
    }

    void graphics_command_builder::set_texture_addressing_mode(drivers::handle h, const drivers::addressing_direction dir, const drivers::addressing_option opt) {
        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_set_texture_wrap;
        cmd->data_[0] = h;
        cmd->data_[1] = PACK_2U32_TO_U64(dir, opt);
    }

    void graphics_command_builder::regenerate_mips(drivers::handle h) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_generate_mips;
        cmd->data_[0] = h;
    }

    void graphics_command_builder::set_swizzle(drivers::handle h, drivers::channel_swizzle r, drivers::channel_swizzle g,
        drivers::channel_swizzle b, drivers::channel_swizzle a) {
        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_set_swizzle;
        cmd->data_[0] = h;
        cmd->data_[1] = PACK_2U32_TO_U64(r, g);
        cmd->data_[2] = PACK_2U32_TO_U64(b, a);
    }

    void graphics_command_builder::set_swapchain_size(const eka2l1::vec2 &swsize) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_swapchain_size;
        cmd->data_[0] = PACK_2U32_TO_U64(swsize.x, swsize.y);
    }

    void graphics_command_builder::set_ortho_size(const eka2l1::vec2 &osize) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_ortho_size;
        cmd->data_[0] = PACK_2U32_TO_U64(osize.x, osize.y);
    }

    void graphics_command_builder::set_point_size(const std::uint8_t value) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_point_size;
        cmd->data_[0] = static_cast<std::uint64_t>(value);
    }

    void graphics_command_builder::set_pen_style(const pen_style style) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_pen_style;
        cmd->data_[0] = static_cast<std::uint64_t>(style);
    }

    void graphics_command_builder::draw_line(const eka2l1::point &start, const eka2l1::point &end) {
        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_draw_line;
        cmd->data_[0] = PACK_2U32_TO_U64(start.x, start.y);
        cmd->data_[1] = PACK_2U32_TO_U64(end.x, end.y);
    }

    void graphics_command_builder::draw_polygons(const eka2l1::point *point_list, const std::size_t point_count) {
        eka2l1::point *point_list_copied = new eka2l1::point[point_count];
        memcpy(point_list_copied, point_list, point_count * sizeof(eka2l1::point));

        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_draw_polygon;
        cmd->data_[0] = point_count;
        cmd->data_[1] = reinterpret_cast<std::uint64_t>(point_list_copied);
    }

    void graphics_command_builder::set_cull_face(const rendering_face face) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_cull_face;
        cmd->data_[0] = static_cast<std::uint64_t>(face);
    }

    void graphics_command_builder::set_front_face_rule(const rendering_face_determine_rule rule) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_front_face_rule;
        cmd->data_[0] = static_cast<std::uint64_t>(rule);
    }

    void graphics_command_builder::set_depth_mask(const std::uint32_t mask) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_depth_set_mask;
        cmd->data_[0] = static_cast<std::uint64_t>(mask);
    }

    void graphics_command_builder::set_depth_pass_condition(const condition_func func) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_depth_func;
        cmd->data_[0] = static_cast<std::uint64_t>(func);
    }

    void graphics_command_builder::recreate_texture(drivers::handle h, const std::uint8_t dim, const std::uint8_t mip_levels,
        drivers::texture_format internal_format, drivers::texture_format data_format, drivers::texture_data_type data_type,
        const void *data, const std::size_t data_size, const eka2l1::vec3 &size, const std::size_t pixels_per_line,
        const std::uint32_t unpack_alignment) {
        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_create_texture;
        cmd->data_[0] = dim | (static_cast<std::uint64_t>(mip_levels) << 8) | (static_cast<std::uint64_t>(internal_format) << 16)
            | (static_cast<std::uint64_t>(data_format) << 32) | (static_cast<std::uint64_t>(data_type) << 48);
        cmd->data_[1] = make_data_copy(data, data_size);
        cmd->data_[2] = data_size;
        cmd->data_[3] = pixels_per_line;
        cmd->data_[4] = static_cast<std::uint64_t>(unpack_alignment);
        cmd->data_[5] = PACK_2U32_TO_U64(size.x, size.y);
        cmd->data_[6] = PACK_2U32_TO_U64(size.z, 0);
        cmd->data_[7] = h;
    }
    
    void graphics_command_builder::recreate_buffer(drivers::handle h, const void *initial_data, const std::size_t initial_size, const buffer_upload_hint upload_hint) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_create_buffer;
        cmd->data_[0] = make_data_copy(initial_data, initial_size);
        cmd->data_[1] = initial_size;
        cmd->data_[2] = static_cast<std::uint64_t>(upload_hint);
        cmd->data_[3] = h;
        cmd->data_[4] = reinterpret_cast<std::uint64_t>(&h);
    }

    void graphics_command_builder::set_color_mask(const std::uint8_t mask) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_color_mask;
        cmd->data_[0] = static_cast<std::uint64_t>(mask);
    }

    void graphics_command_builder::set_texture_for_shader(const int texture_slot, const int shader_binding, const drivers::shader_module_type module) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_texture_for_shader;
    
        cmd->data_[0] = PACK_2U32_TO_U64(texture_slot, shader_binding);
        cmd->data_[1] = static_cast<std::uint64_t>(module);
    }

    void graphics_command_builder::update_input_descriptors(drivers::handle h, input_descriptor *descriptors, const std::uint32_t count) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_create_input_descriptor;
    
        cmd->data_[0] = make_data_copy(descriptors, count * sizeof(input_descriptor));
        cmd->data_[1] = count;
        cmd->data_[2] = h;
        cmd->data_[3] = reinterpret_cast<std::uint64_t>(&h);
    }

    void graphics_command_builder::bind_input_descriptors(drivers::handle h) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_bind_input_descriptor;
        cmd->data_[0] = h;
    }

    void graphics_command_builder::set_line_width(const float width) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_line_width;
        cmd->data_[0] = *reinterpret_cast<const std::uint32_t*>(&width);
    }

    void graphics_command_builder::set_texture_max_mip(drivers::handle h, const std::uint32_t max_mip) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_max_mip_level;
        cmd->data_[0] = h;
        cmd->data_[1] = static_cast<std::uint64_t>(max_mip);
    }

    void graphics_command_builder::set_depth_bias(float constant_factor, float clamp, float slope_factor) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_depth_bias;
        cmd->data_[0] = pack_from_two_floats(constant_factor, slope_factor);
        cmd->data_[1] = pack_from_two_floats(clamp, 0);
    }

    void graphics_command_builder::set_depth_range(const float min, const float max) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_depth_range;
        cmd->data_[0] = pack_from_two_floats(min, max);
    }
    
    void graphics_command_builder::set_texture_anisotrophy(drivers::handle h, const float anisotrophy_fact) {
        command *cmd = list_.retrieve_next();
        cmd->opcode_ = graphics_driver_set_texture_anisotrophy;
        cmd->data_[0] = h;
        cmd->data_[1] = pack_from_two_floats(anisotrophy_fact, 0);
    }
    
    void graphics_command_builder::recreate_renderbuffer(drivers::handle h, const eka2l1::vec2 &size, const drivers::texture_format internal_format) {
        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_create_renderbuffer;
        cmd->data_[0] = PACK_2U32_TO_U64(size.x, size.y);
        cmd->data_[1] = static_cast<std::uint64_t>(internal_format);
        cmd->data_[2] = h;
    }

    void graphics_command_builder::set_framebuffer_color_buffer(drivers::handle h, drivers::handle color_buffer, const int face_index, const std::int32_t color_index) {
        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_set_framebuffer_color_buffer;
        cmd->data_[0] = h;
        cmd->data_[1] = color_buffer;
        cmd->data_[2] = static_cast<std::uint64_t>(face_index);
        cmd->data_[3] = static_cast<std::uint64_t>(color_index);
    }

    void graphics_command_builder::set_framebuffer_depth_stencil_buffer(drivers::handle h, drivers::handle depth, const int depth_face_index, drivers::handle stencil, const int stencil_face_index) {
        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_set_framebuffer_depth_stencil_buffer;
        cmd->data_[0] = h;
        cmd->data_[1] = depth;
        cmd->data_[2] = stencil;
        cmd->data_[3] = PACK_2U32_TO_U64(depth_face_index, stencil_face_index);
    }

    void graphics_command_builder::bind_framebuffer(drivers::handle h, drivers::framebuffer_bind_type bind_type) {
        command *cmd = list_.retrieve_next();

        cmd->opcode_ = graphics_driver_bind_framebuffer;
        cmd->data_[0] = h;
        cmd->data_[1] = static_cast<std::uint64_t>(bind_type);
    }

    void advance_draw_pos_around_origin(eka2l1::rect &origin_normal_rect, const int rotation) {
        switch (rotation) {
        case 90:
            origin_normal_rect.top.x += origin_normal_rect.size.x;
            break;

        case 180:
            origin_normal_rect.top.x += origin_normal_rect.size.x;
            origin_normal_rect.top.y += origin_normal_rect.size.y;
            break;

        case 270:
            origin_normal_rect.top.y += origin_normal_rect.size.y;
            break;

        default:
            break;
        }
    }
}
