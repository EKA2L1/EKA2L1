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
#include <epoc/services/window/window.h>
#include <epoc/services/window/classes/winbase.h>
#include <epoc/services/window/classes/wingroup.h>
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

            if (!winuser->irect.empty()) {
                // Wakeup windows, we have a region to invalidate
                // Yes, I'm referencing a meme. Send help.
                // The invalidate region is there. Gone with what we have first, but do redraw still
                winuser->client->queue_redraw(winuser, winuser->irect);
            }

            // Draw it onto current binding buffer
            builder_->draw_bitmap(winuser->driver_win_id, winuser->pos, eka2l1::rect({0, 0}, { 0, 0 }), false);
            return false;
        }
    };

    screen::screen(const int number, epoc::config::screen &scr_conf) 
        : number(number)
        , screen_texture(0)
        , scr_config(scr_conf)
        , crr_mode(1)
        , next(nullptr)
        , focus(nullptr) {
        root = std::make_unique<epoc::window>(nullptr, this, nullptr);
    }

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

    void screen::deinit(drivers::graphics_driver *driver) {
        // Make command list first, and bind our screen bitmap
        auto cmd_list = driver->new_command_list();
        auto cmd_builder = driver->new_command_builder(cmd_list.get());

        if (screen_texture) {
            cmd_builder->destroy_bitmap(screen_texture);
        }

        driver->submit_command_list(*cmd_list);
    }

    void screen::resize(drivers::graphics_driver *driver, const eka2l1::vec2 &new_size) {
        // Make command list first, and bind our screen bitmap
        auto cmd_list = driver->new_command_list();
        auto cmd_builder = driver->new_command_builder(cmd_list.get());

        if (!screen_texture) {
            // Create new one!
            screen_texture = drivers::create_bitmap(driver, new_size);
        } else {    
            cmd_builder->resize_bitmap(screen_texture, new_size);
        }

        redraw(cmd_builder.get());
        driver->submit_command_list(*cmd_list);
    }

    epoc::window_group *screen::update_focus(epoc::window_group *closing_group) {
        // Iterate through root's childs.
        epoc::window_group *next_to_focus = reinterpret_cast<epoc::window_group*>(root->child);

        while (next_to_focus != nullptr) {
            assert(next_to_focus->type == window_kind::group && "The window kind of lvl1 root's child must be window group");
            if (next_to_focus->can_receive_focus())
                break;
            
            next_to_focus = reinterpret_cast<epoc::window_group*>(next_to_focus->sibling);
        }

        if (next_to_focus != focus) {
            if (focus && focus != closing_group) {
                focus->lost_focus();
            }

            if (next_to_focus) {
                next_to_focus->gain_focus();
                focus = std::move(next_to_focus);
            }
        }

        return next_to_focus;
    }

    void screen::set_screen_mode(drivers::graphics_driver *drv, const int mode) {
        crr_mode = mode;
        resize(drv, mode_info(mode)->size);
    }

    const epoc::config::screen_mode *screen::mode_info(const int number) const {
        for (std::size_t i = 0; i < scr_config.modes.size(); i++) {
            if (scr_config.modes[i].mode_number == number) {
                return &scr_config.modes[i];
            }
        }

        return nullptr;
    }
    
    const epoc::config::screen_mode &screen::current_mode() const {
        return *mode_info(crr_mode);
    }

    eka2l1::vec2 screen::size() const {
        return current_mode().size;
    }

    const int screen::total_screen_mode() const {
        return static_cast<int>(scr_config.modes.size());
    }
}