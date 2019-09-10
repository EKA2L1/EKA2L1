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

using namespace std::chrono_literals;

namespace eka2l1::drivers {
    static int send_sync_command_detail(graphics_driver *drv, command *cmd) {
        int status = -100;
        cmd->status_ = &status;

        command_list cmd_list;
        cmd_list.add(cmd);

        server_graphics_command_list gcmd_list;
        gcmd_list.list_ = cmd_list;

        std::unique_lock<std::mutex> ulock(drv->mut_);
        drv->submit_command_list(gcmd_list);
        drv->cond_.wait(ulock, [&]() { return status != -100; });

        return status;
    }

    template <typename T, typename... Args>
    static int send_sync_command(T drv, const std::uint16_t opcode, Args... args) {
        command *cmd = make_command(opcode, nullptr, args...);
        return send_sync_command_detail(drv, cmd);
    }

    static void *make_data_copy(const void *source, const std::size_t size) {
        std::uint8_t *copy = new std::uint8_t[size];
        std::copy(reinterpret_cast<const std::uint8_t *>(source), reinterpret_cast<const std::uint8_t *>(source) + size, copy);

        return copy;
    }

    drivers::handle create_bitmap(graphics_driver *driver, const eka2l1::vec2 &size) {
        drivers::handle handle_num = 0;

        if (send_sync_command(driver, graphics_driver_create_bitmap, size.x, size.y, &handle_num) != 0) {
            return 0;
        }

        return handle_num;
    }

    drivers::handle create_program(graphics_driver *driver, const char *vert_data, const std::size_t vert_size,
        const char *frag_data, const std::size_t frag_size, shader_metadata *metadata) {
        drivers::handle handle_num = 0;
        std::uint8_t *metadata_ptr = nullptr;

        if (send_sync_command(driver, graphics_driver_create_program, vert_data, frag_data, vert_size, frag_size, &metadata_ptr, &handle_num) != 0) {
            return 0;
        }

        if (metadata)
            metadata->metadata_ = metadata_ptr;

        return handle_num;
    }

    drivers::handle create_buffer(graphics_driver *driver, const std::size_t initial_size, const buffer_hint hint,
        const buffer_upload_hint upload_hint) {
        drivers::handle handle_num = 0;

        if (send_sync_command(driver, graphics_driver_create_buffer, initial_size, hint, upload_hint, &handle_num) != 0) {
            return 0;
        }

        return handle_num;
    }

    drivers::handle create_texture(graphics_driver *driver, const std::uint8_t dim, const std::uint8_t mip_levels,
        drivers::texture_format internal_format, drivers::texture_format data_format, drivers::texture_data_type data_type,
        const void *data, const eka2l1::vec3 &size) {
        drivers::handle handle_num = 0;

        switch (dim) {
        case 1:
            if (send_sync_command(driver, graphics_driver_create_texture, dim, mip_levels, internal_format, data_format, data_type, data, size.x, &handle_num) != 0) {
                return 0;
            }

            break;

        case 2:
            if (send_sync_command(driver, graphics_driver_create_texture, dim, mip_levels, internal_format, data_format, data_type, data, size.x, size.y, &handle_num) != 0) {
                return 0;
            }

            break;

        case 3:
            if (send_sync_command(driver, graphics_driver_create_texture, dim, mip_levels, internal_format, data_format, data_type, data, size.x, size.y, size.z, &handle_num) != 0) {
                return 0;
            }

            break;

        default:
            break;
        }

        return handle_num;
    }

    server_graphics_command_list_builder::server_graphics_command_list_builder(graphics_command_list *cmd_list)
        : graphics_command_list_builder(cmd_list) {
    }

    void server_graphics_command_list_builder::invalidate_rect(eka2l1::rect &rect) {
        command *cmd = make_command(graphics_driver_invalidate_rect, nullptr, rect.top.x, rect.top.y,
            rect.size.x, rect.size.y);

        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::set_invalidate(const bool enabled) {
        command *cmd = make_command(graphics_driver_invalidate_rect, nullptr, enabled);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::clear(vecx<std::uint8_t, 4> color, const std::uint8_t clear_bitarr) {
        command *cmd = make_command(graphics_driver_clear, nullptr, color[0], color[1], color[2], color[3], clear_bitarr);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::resize_bitmap(drivers::handle h, const eka2l1::vec2 &new_size) {
        // This opcode has two variant: sync or async.
        // The first argument is bitmap handle. If it's null then the currently binded one will be used.
        command *cmd = make_command(graphics_driver_resize_bitmap, nullptr, h, new_size);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::update_bitmap(drivers::handle h, const int bpp, const char *data, const std::size_t size,
        const eka2l1::vec2 &offset, const eka2l1::vec2 &dim) {
        // Copy data
        command *cmd = make_command(graphics_driver_update_bitmap, nullptr, make_data_copy(data, size), bpp, size, offset, dim, bpp);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::draw_bitmap(drivers::handle h, const eka2l1::rect &dest_rect, const bool use_brush) {
        command *cmd = make_command(graphics_driver_draw_bitmap, nullptr, h, dest_rect.top, dest_rect.size, use_brush);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::bind_bitmap(const drivers::handle h) {
        command *cmd = make_command(graphics_driver_bind_bitmap, nullptr, h);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::set_brush_color(const eka2l1::vec3 &color) {
        command *cmd = make_command(graphics_driver_set_brush_color, nullptr, static_cast<float>(color.x),
            static_cast<float>(color.y), static_cast<float>(color.z), 1.0f);

        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::use_program(drivers::handle h) {
        command *cmd = make_command(graphics_driver_use_program, nullptr, h);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::set_uniform(drivers::handle h, const int binding, const drivers::shader_set_var_type var_type,
        const void *data, const std::size_t data_size) {
        const void *uniform_data = make_data_copy(data, data_size);

        command *cmd = make_command(graphics_driver_set_uniform, nullptr, h, var_type, uniform_data, binding);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::bind_texture(drivers::handle h, const int binding) {
        command *cmd = make_command(graphics_driver_bind_texture, nullptr, h, binding);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::draw_indexed(const graphics_primitive_mode prim_mode, const int count, const data_format index_type, const int index_off, const int vert_base) {
        command *cmd = make_command(graphics_driver_draw_indexed, nullptr, prim_mode, count, index_type, index_off, vert_base);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::bind_buffer(drivers::handle h) {
        command *cmd = make_command(graphics_driver_bind_buffer, nullptr, h);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::update_buffer_data(drivers::handle h, const std::size_t offset, const int chunk_count, const void **chunk_ptr, const std::uint16_t *chunk_size) {
        std::uint32_t total_chunk_size = 0;
        std::uint32_t cursor = 0;

        for (int i = 0; i < chunk_count; i++) {
            total_chunk_size += chunk_size[i];
        }

        std::uint8_t *data = new std::uint8_t[total_chunk_size];

        for (int i = 0; i < chunk_count; i++) {
            std::copy(reinterpret_cast<const std::uint8_t *>(chunk_ptr[i]), reinterpret_cast<const std::uint8_t *>(chunk_ptr[i]) + chunk_size[i], data + cursor);
            cursor += chunk_size[i];
        }

        command *cmd = make_command(graphics_driver_update_buffer, nullptr, h, data, offset, static_cast<std::size_t>(total_chunk_size));
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::set_viewport(const eka2l1::rect &viewport_rect) {
        command *cmd = make_command(graphics_driver_set_viewport, nullptr, viewport_rect);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::create_single_set_command(const std::uint16_t op, const bool enable) {
        command *cmd = make_command(op, nullptr, enable);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::set_depth(const bool enable) {
        create_single_set_command(graphics_driver_set_depth, enable);
    }

    void server_graphics_command_list_builder::set_cull_mode(const bool enable) {
        create_single_set_command(graphics_driver_set_cull, enable);
    }

    void server_graphics_command_list_builder::set_blend_mode(const bool enable) {
        create_single_set_command(graphics_driver_set_blend, enable);
    }

    void server_graphics_command_list_builder::blend_formula(const blend_equation rgb_equation, const blend_equation a_equation,
        const blend_factor rgb_frag_output_factor, const blend_factor rgb_current_factor,
        const blend_factor a_frag_output_factor, const blend_factor a_current_factor) {
        command *cmd = make_command(graphics_driver_blend_formula, nullptr, rgb_equation, a_equation, rgb_frag_output_factor,
            rgb_current_factor, a_frag_output_factor, a_current_factor);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::backup_state() {
        command *cmd = make_command(graphics_driver_backup_state, nullptr);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::load_backup_state() {
        command *cmd = make_command(graphics_driver_restore_state, nullptr);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::attach_descriptors(drivers::handle h, const int stride, const bool instance_move,
        const attribute_descriptor *descriptors, const int descriptor_count) {
        void *des = make_data_copy(descriptors, descriptor_count * sizeof(attribute_descriptor));
        command *cmd = make_command(graphics_driver_attach_descriptors, nullptr, h, stride, instance_move, des, descriptor_count);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::present(int *status) {
        command *cmd = make_command(graphics_driver_display, status);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::destroy(drivers::handle h) {
        command *cmd = make_command(graphics_driver_destroy_object, nullptr, h);
        get_command_list().add(cmd);
    }

    void server_graphics_command_list_builder::destroy_bitmap(drivers::handle h) {
        command *cmd = make_command(graphics_driver_destroy_bitmap, nullptr, h);
        get_command_list().add(cmd);
    }
    
    void server_graphics_command_list_builder::set_texture_filter(drivers::handle h, const drivers::filter_option min, const drivers::filter_option mag) {
        command *cmd = make_command(graphics_driver_set_texture_filter, nullptr, h);
        get_command_list().add(cmd);
    }
}