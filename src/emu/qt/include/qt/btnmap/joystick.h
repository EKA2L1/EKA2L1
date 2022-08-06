/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#pragma once

#include <qt/btnmap/base.h>
#include <qt/btnmap/singletouch.h>

#include <memory>

namespace eka2l1::qt::btnmap {
    class joystick: public base {
    private:
        friend class editor;

        std::unique_ptr<single_touch> keys_[5];  // Up, down, left, right, Joy
        drivers::handle base_tex_handle_;
        std::uint32_t width_;
        eka2l1::rect previous_rect_;

        int focusing_key_;
        int resizing_mode_;

        void positioning_bind_buttons();
        void on_dragging(const eka2l1::vec2 &current_pos) override;

    public:
        explicit joystick(editor *editor_instance, const eka2l1::vec2 &position);

        void draw(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder,
                  const float scale_factor) override;

        void on_key_press(const std::uint32_t code) override;
        void on_controller_button_press(const std::uint32_t code) override;
        void on_joy_move(const std::uint32_t code, const float axisx, const float axisy) override;

        action_result on_mouse_action(const eka2l1::vec2 &value, const int action, const bool is_selected) override;
        bool in_content_area(const eka2l1::vec2 &pos) const override;

        editor_entity_type entity_type() const override {
            return EDITOR_ENTITY_TYPE_JOYSTICK;
        }
    };
}
