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

#include <services/window/classes/winbase.h>
#include <services/window/classes/wingroup.h>
#include <services/window/classes/winuser.h>
#include <services/window/screen.h>
#include <services/window/window.h>

#include <common/time.h>
#include <drivers/itc.h>

#include <kernel/kernel.h>
#include <kernel/timing.h>
#include <thread>

namespace eka2l1::epoc {
    struct window_drawer_walker : public window_tree_walker {
        drivers::graphics_command_list_builder *builder_;
        std::uint32_t total_redrawed_;

        explicit window_drawer_walker(drivers::graphics_command_list_builder *builder)
            : builder_(builder)
            , total_redrawed_(0) {
        }

        bool do_it(window *win) {
            if (win->type != window_kind::client) {
                return false;
            }

            window_user *winuser = reinterpret_cast<window_user *>(win);

            if (!winuser || !winuser->driver_win_id || !winuser->is_visible()) {
                // No need to redraw this window yet. It doesn't even have any content ready.
                return false;
            }

            // Check if extent is just invalid
            if (winuser->size.x == 0 || winuser->size.y == 0) {
                // No one can see this. Leave it for now.
                return false;
            }

            // Draw it onto current binding buffer
            builder_->draw_bitmap(winuser->driver_win_id, 0, eka2l1::rect(winuser->pos, { 0, 0 }),
                eka2l1::rect({ 0, 0 }, winuser->size), 0);

            total_redrawed_++;

            return false;
        }
    };

    screen::screen(const int number, epoc::config::screen &scr_conf)
        : number(number)
        , ui_rotation(0)
        , orientation_lock(true)
        , refresh_rate(30)
        , scale_x(1.0f)
        , scale_y(1.0f)
        , screen_texture(0)
        , dsa_texture(0)
        , disp_mode(display_mode::color16ma)
        , last_vsync(0)
        , scr_config(scr_conf)
        , crr_mode(1)
        , next(nullptr)
        , focus(nullptr) {
        root = std::make_unique<epoc::window>(nullptr, this, nullptr);
        disp_mode = scr_conf.disp_mode;

        for (std::size_t i = 0; i < scr_config.modes.size(); i++) {
            if (scr_config.modes[i].rotation == 0) {
                physical_mode = static_cast<std::uint8_t>(i + 1);
            }
        }
    }

    void screen::redraw(drivers::graphics_command_list_builder *cmd_builder, const bool need_bind) {
        if (need_bind) {
            cmd_builder->bind_bitmap(screen_texture);
        }

        // Walk through the window tree in recursive order, and do draw
        // We dont care about visible regions. Nowadays, detect visible region to reduce pixel plotting is
        // just not really worth the time, since GPU draws so fast. Symbian code still has it though.
        window_drawer_walker adrawwalker(cmd_builder);
        root->walk_tree_back_to_front(&adrawwalker);

        // Done! Unbind and submit this to the driver
        cmd_builder->bind_bitmap(0);
    }

    void screen::redraw(drivers::graphics_driver *driver) {
        if (!screen_texture) {
            set_screen_mode(driver, crr_mode);
        }

        // Make command list first, and bind our screen bitmap
        auto cmd_list = driver->new_command_list();
        auto cmd_builder = driver->new_command_builder(cmd_list.get());
        redraw(cmd_builder.get(), true);
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

    const void screen::get_max_num_colors(int &colors, int &greys) const {
        greys = 0;
        colors = get_num_colors_from_display_mode(disp_mode);
        if (!is_display_mode_color(disp_mode)) {
            greys = colors;
            colors = 0;
        }
    }

    void screen::resize(drivers::graphics_driver *driver, const eka2l1::vec2 &new_size) {
        // Make command list first, and bind our screen bitmap
        auto cmd_list = driver->new_command_list();
        auto cmd_builder = driver->new_command_builder(cmd_list.get());

        bool need_bind = true;

        if (!screen_texture) {
            // Create new one!
            screen_texture = drivers::create_bitmap(driver, new_size, 32);
        } else {
            cmd_builder->bind_bitmap(screen_texture);
            cmd_builder->resize_bitmap(screen_texture, new_size);

            need_bind = false;
        }

        redraw(cmd_builder.get(), need_bind);
        driver->submit_command_list(*cmd_list);
    }

    static epoc::window_group *find_group_to_focus(epoc::window *root) {
        epoc::window_group *next_to_focus = reinterpret_cast<epoc::window_group *>(root->child);

        while (next_to_focus != nullptr) {
            assert(next_to_focus->type == window_kind::group && "The window kind of lvl1 root's child must be window group");
            if (next_to_focus->can_receive_focus())
                break;

            next_to_focus = reinterpret_cast<epoc::window_group *>(next_to_focus->sibling);
        }

        return next_to_focus;
    }

    epoc::window_group *screen::update_focus(window_server *serv, epoc::window_group *closing_group) {
        epoc::window_group *old_focus = focus;

        // Iterate through root's childs.
        focus = find_group_to_focus(root.get());
        const bool is_me_currently_focus = (serv->get_current_focus_screen() == this);

        epoc::window_group *alternative_focus = nullptr;
        screen *new_focus_screen = nullptr;

        if (is_me_currently_focus && !focus) {
            // Don't give up hope! There must be one group to be focused, and it may be present
            // in other screen.
            // iterate through all screen
            new_focus_screen = serv->get_screens();

            while (new_focus_screen != nullptr) {
                if (new_focus_screen != this) {
                    // Try finding out a new group
                    alternative_focus = find_group_to_focus(new_focus_screen->root.get());
                }

                if (alternative_focus) {
                    break;
                }

                new_focus_screen = new_focus_screen->next;
            }

            if (!alternative_focus) {
                new_focus_screen = nullptr;
            }
        }

        if (old_focus != focus || new_focus_screen) {
            if (old_focus && old_focus != closing_group && is_me_currently_focus) {
                focus->lost_focus();
            }

            if (new_focus_screen) {
                serv->set_focus_screen(new_focus_screen);
                alternative_focus->gain_focus();
                new_focus_screen->fire_focus_change_callbacks();
            } else if (focus && is_me_currently_focus) {
                focus->gain_focus();
                fire_focus_change_callbacks();
            }
        }

        return (new_focus_screen ? alternative_focus : focus);
    }

    void screen::fire_focus_change_callbacks() {
        const std::lock_guard<std::mutex> guard(screen_mutex);

        for (auto &callback : focus_callbacks) {
            callback.second(callback.first, focus);
        }
    }

    std::size_t screen::add_focus_change_callback(void *userdata, focus_change_callback_handler handler) {
        const std::lock_guard<std::mutex> guard(screen_mutex);
        focus_callbacks.push_back({ userdata, handler });

        return focus_callbacks.size();
    }

    void screen::set_screen_mode(drivers::graphics_driver *drv, const int mode) {
        crr_mode = mode;
        resize(drv, mode_info(mode)->size);
    }

    void screen::vsync(ntimer *timing, std::uint64_t &next_vsync_us) {
        const std::uint64_t tnow = common::get_current_time_in_microseconds_since_epoch();
        std::uint64_t delta = tnow - last_vsync;

        const std::uint64_t microsecs_a_frame = 1000000 / refresh_rate;

        if (delta < microsecs_a_frame) {
            next_vsync_us = microsecs_a_frame - delta;
        } else {
            next_vsync_us = 0;
        }

        last_vsync = tnow;
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
        return mode_info(physical_mode)->size;
    }

    const int screen::total_screen_mode() const {
        return static_cast<int>(scr_config.modes.size());
    }

    void screen::set_rotation(drivers::graphics_driver *drv, const int rot) {
        if (orientation_lock) {
            // Feel like we are at home
            ui_rotation = rot;
        } else {
            // We need to confirm this rotation exists
            for (std::size_t i = 0; i < scr_config.modes.size(); i++) {
                if (scr_config.modes[i].rotation == rot) {
                    // Reset the UI rotation
                    ui_rotation = 0;
                    set_screen_mode(drv, static_cast<int>(i));
                }
            }
        }
    }

    void screen::set_orientation_lock(drivers::graphics_driver *drv, const bool lock) {
        if (orientation_lock == lock) {
            return;
        }

        if (orientation_lock && !lock) {
            // Transitioning to wild mode
            orientation_lock = false;
            return;
        }

        // Abadon our UI rotation
        ui_rotation = 0;
        orientation_lock = true;
    }
}