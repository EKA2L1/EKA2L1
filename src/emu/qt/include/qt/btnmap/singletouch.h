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

namespace eka2l1::qt::btnmap {
    class single_touch: public base {
    private:
        friend class editor;

        std::uint32_t keycode_;
        bool waiting_key_;

        eka2l1::rect get_content_box(const std::string &keystr) const;
        const std::string get_readable_keyname() const;

    public:
        explicit single_touch(editor *editor_instance, const eka2l1::vec2 &position);

        void draw(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder,
                  const float scale_factor) override;

        std::uint32_t keycode() const {
            return keycode_;
        }

        void keycode(const std::uint32_t new_code) {
            keycode_ = new_code;
        }

        editor_entity_type entity_type() const override {
            return EDITOR_ENTITY_TYPE_SINGLE_TOUCH;
        }

        action_result on_mouse_action(const eka2l1::vec2 &value, const int action, const bool is_selected) override;
        
        void on_key_press(const std::uint32_t code) override;
        void on_controller_button_press(const std::uint32_t code) override;
        bool in_content_area(const eka2l1::vec2 &pos) const override;
        void update_key_wait_statuses(const bool selected);
    };
}
