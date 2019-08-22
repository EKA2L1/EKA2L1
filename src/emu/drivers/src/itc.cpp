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
#include <drivers/itc.h>

#include <drivers/graphics/graphics.h>
#include <drivers/input/input.h>

#include <chrono>

using namespace std::chrono_literals;

namespace eka2l1::drivers {
    static int send_sync_command_detail(driver *drv, command *cmd) {
        int status = -100;
        cmd->status_ = &status;

        command_list cmd_list;
        cmd_list.add(cmd);

        std::unique_lock<std::mutex> ulock(drv->mut_);
        drv->add_list(cmd_list);
        drv->cond_.wait(ulock, [&]() { return status != -100; });

        return status;
    }

    template <typename T, typename... Args>
    static int send_sync_command(T drv, const std::uint16_t opcode, Args... args) {
        command *cmd = make_command(opcode, nullptr, args...);
        return send_sync_command_detail(drv, cmd);
    }

    static void *make_data_copy(const void *source, const std::size_t size) {
        void *copy = new std::uint8_t[size];
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
        const char *frag_data, const std::size_t frag_size) {
        drivers::handle handle_num = 0;

        if (send_sync_command(driver, graphics_driver_create_program, vert_data, frag_data, vert_size, frag_size, &handle_num) != 0) {
            return 0;
        }

        return handle_num;
    }

    drivers::handle create_texture(graphics_driver *driver, const std::uint8_t dim, const std::uint8_t mip_levels,
        drivers::texture_format internal_format, drivers::texture_format data_format, drivers::texture_format data_type,
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

        get_command_list()->add(cmd);
    }

    void server_graphics_command_list_builder::set_invalidate(const bool enabled) {
        command *cmd = make_command(graphics_driver_invalidate_rect, nullptr, enabled);
        get_command_list()->add(cmd);
    }

    void server_graphics_command_list_builder::clear(vecx<int, 4> color) {
        command *cmd = make_command(graphics_driver_clear, nullptr, color[0], color[1], color[2], color[3]);
        get_command_list()->add(cmd);
    }

    void server_graphics_command_list_builder::resize_bitmap(const eka2l1::vec2 &new_size) {
        // This opcode has two variant: sync or async.
        // The first argument is bitmap handle. If it's null then the currently binded one will be used.
        command *cmd = make_command(graphics_driver_resize_bitmap, nullptr, 0, new_size);
        get_command_list()->add(cmd);
    }

    void server_graphics_command_list_builder::update_bitmap(drivers::handle h, const int bpp, const char *data, const std::size_t size,
        const eka2l1::vec2 &offset, const eka2l1::vec2 &dim) {
        // Copy data
        command *cmd = make_command(graphics_driver_update_bitmap, nullptr, make_data_copy(data, size), bpp, size, offset, dim, bpp);
        get_command_list()->add(cmd);
    }

    void server_graphics_command_list_builder::draw_bitmap(drivers::handle h, const eka2l1::rect &dest_rect, const bool use_brush) {
        command *cmd = make_command(graphics_driver_draw_bitmap, nullptr, h, dest_rect.top, dest_rect.size, use_brush);
        get_command_list()->add(cmd);
    }

    void server_graphics_command_list_builder::bind_bitmap(const drivers::handle h) {
        command *cmd = make_command(graphics_driver_bind_bitmap, nullptr, h);
        get_command_list()->add(cmd);
    }

    void server_graphics_command_list_builder::set_brush_color(const eka2l1::vec3 &color) {
        command *cmd = make_command(graphics_driver_set_brush_color, nullptr, static_cast<float>(color.x),
            static_cast<float>(color.y), static_cast<float>(color.z), 1.0f);

        get_command_list()->add(cmd);
    }
    /*
        input_driver_client::input_driver_client(driver_instance driver)
            : driver_client(driver) {
            should_timeout = true;
        }

        void input_driver_client::get(input_event *evt, const std::uint32_t total_to_get) {
            itc_context context;
            context.push(evt);
            context.push(total_to_get);

            send_opcode_sync(input_driver_get_events, context);
        }

        void input_driver_client::lock() {
            itc_context context;
            send_opcode_sync(input_driver_lock, context);
        }

        void input_driver_client::release() {
            itc_context context;
            send_opcode_sync(input_driver_release, context);
        }

        std::uint32_t input_driver_client::total() {
            std::uint32_t total = 0;

            itc_context context;
            context.push(&total);

            send_opcode_sync(input_driver_get_total_events, context);
            return total;
        }*/
}
}