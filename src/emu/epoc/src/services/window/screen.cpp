/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <epoc/services/window/screen.h>
#include <epoc/services/window/classes/winbase.h>
#include <epoc/services/window/classes/winuser.h>

#include <drivers/itc.h>

namespace eka2l1::epoc {
    struct window_drawer_walker: public window_tree_walker {
        drivers::graphics_command_list_builder *builder_;

        explicit window_drawer_walker(drivers::graphics_command_list_builder *builder)
            : builder_(builder) {
        }

        bool do_it(window *win) {
            assert(win->type == window_kind::client && "Window to draw must have kind of user");
            window_user *winuser = reinterpret_cast<window_user*>(win);

            // Draw it onto current binding buffer
            builder_->draw_bitmap(winuser->driver_win_id, eka2l1::rect({0, 0}, winuser->size), false);
            return false;
        }
    };

    void screen::redraw(drivers::graphics_command_list_builder *cmd_builder) {
        cmd_builder->bind_bitmap(screen_texture);

        // Walk through the window tree in recursive order, and do draw
        // We dont care about visible regions. Nowadays, detect visible region to reduce pixel plotting is
        // just not really worth the time, since GPU draws so fast. Symbian code still has it though.
        window_drawer_walker adrawwalker(cmd_builder);
        root->walk_tree_back_to_front(&adrawwalker);

        // Done! Unbind and submit this to the driver
        cmd_builder->bind_bitmap(0);
    }
    
    void screen::redraw(drivers::graphics_driver *driver) {
        // Make command list first, and bind our screen bitmap
        auto cmd_list = driver->new_command_list();
        auto cmd_builder = driver->new_command_builder(cmd_list.get());
        redraw(cmd_builder.get());
        driver->submit_command_list(*cmd_list);
    }

    void screen::resize(drivers::graphics_driver *driver, const eka2l1::vec2 &new_size) {
        // Make command list first, and bind our screen bitmap
        auto cmd_list = driver->new_command_list();
        auto cmd_builder = driver->new_command_builder(cmd_list.get());

        size = new_size;

        if (!screen_texture) {
            // Create new one!
            screen_texture = drivers::create_bitmap(driver, new_size);
        } else {    
            cmd_builder->resize_bitmap(screen_texture, new_size);
        }

        redraw(cmd_builder.get());
        driver->submit_command_list(*cmd_list);
    }
}