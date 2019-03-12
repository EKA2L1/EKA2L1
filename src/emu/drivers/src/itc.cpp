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
    bool driver_client::send_opcode_sync(const int opcode, itc_context &ctx) {
        if (!driver) {
            return false;
        }

        if (already_locked) {
            driver->request_queue.push_unsafe({ opcode, ctx });
        } else {
            driver->request_queue.push({ opcode, ctx });
        }

        sync_with_driver();

        return true;
    }

    bool driver_client::send_opcode(const int opcode, itc_context &ctx) {
        if (!driver) {
            return false;
        }
        
        if (already_locked) {
            driver->request_queue.push_unsafe({ opcode, ctx });
        } else {
            driver->request_queue.push({ opcode, ctx });
        }

        return true;
    }

    void driver_client::lock_driver_from_process() {
        driver->request_queue.lock.lock();
        already_locked = true;
    }

    void driver_client::unlock_driver_from_process() {
        driver->request_queue.lock.unlock();
        already_locked = false;
    }

    void driver_client::sync_with_driver() {
        std::unique_lock<std::mutex> ulock(dri_cli_lock);
        driver->cond.wait_for(ulock, timeout * 1ms);
    }

    driver_client::driver_client(driver_instance driver)
        : driver(driver) {
    }

    graphics_driver_client::graphics_driver_client(driver_instance driver)
        : driver_client(driver) {
    }

    /*! \brief Get the screen size in pixels
    */
    vec2 graphics_driver_client::screen_size() {
        // Synchronize call should directly use the client
        std::shared_ptr<graphics_driver> gdriver = std::reinterpret_pointer_cast<graphics_driver>(driver);

        return gdriver->get_screen_size();
    }

    void graphics_driver_client::set_screen_size(eka2l1::vec2 &s) {
        itc_context context;
        context.push(s);

        send_opcode(graphics_driver_resize_screen, context);
    }

    void graphics_driver_client::invalidate(eka2l1::rect &rect) {
        itc_context context;
        context.push(rect);

        send_opcode(graphics_driver_invalidate, context);
    }

    void graphics_driver_client::end_invalidate() {
        itc_context context;
        send_opcode(graphics_driver_end_invalidate, context);
    }

    std::uint32_t graphics_driver_client::create_window(const eka2l1::vec2 &initial_size, const std::uint16_t pri,
        const bool visible_from_start) {
        std::uint32_t result = 0;

        itc_context context;
        context.push(initial_size);
        context.push(pri);
        context.push(visible_from_start);
        context.push(&result);

        send_opcode_sync(graphics_driver_create_window, context);

        return result;
    }

    /*! \brief Clear the screen with color.
        \params color A RGBA vector 4 color
    */
    void graphics_driver_client::clear(vecx<int, 4> color) {
        itc_context context;
        context.push(color[0]);
        context.push(color[1]);
        context.push(color[2]);
        context.push(color[3]);

        send_opcode(graphics_driver_clear, context);
    }

    void graphics_driver_client::draw_text(eka2l1::rect rect, const std::string &str) {
        itc_context context;
        context.push(rect);
        context.push_string(str);

        send_opcode(graphics_driver_draw_text_box, context);
    }

    void graphics_driver_client::set_window_visible(const std::uint32_t id, const bool visible) {
        itc_context context;
        context.push(id);
        context.push(visible);

        send_opcode_sync(graphics_driver_set_visibility, context);
    }

    void graphics_driver_client::set_window_size(const std::uint32_t id, const eka2l1::vec2 &win_size) {
        itc_context context;
        context.push(id);
        context.push(win_size);

        send_opcode_sync(graphics_driver_set_window_size, context);
    }

    void graphics_driver_client::set_window_priority(const std::uint32_t id, const std::uint16_t pri) {
        itc_context context;
        context.push(id);
        context.push(pri);

        send_opcode_sync(graphics_driver_set_priority, context);
    }

    void graphics_driver_client::set_window_pos(const std::uint32_t id, const eka2l1::vec2 &pos) {
        itc_context context;
        context.push(id);
        context.push(pos);

        send_opcode_sync(graphics_driver_set_win_pos, context);
    }
    
    drivers::handle graphics_driver_client::upload_bitmap(drivers::handle h, const char *data, const std::size_t size,
        const std::uint32_t width, const std::uint32_t height, const int bpp) {
        itc_context context;
        context.push(size);
        context.push(width);
        context.push(height);
        context.push(bpp);
        context.push(data);
        context.push(&h);

        send_opcode_sync(graphics_driver_upload_bitmap, context);
        return h;
    }

    input_driver_client::input_driver_client(driver_instance driver)
        : driver_client(driver) {
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
    }
}
