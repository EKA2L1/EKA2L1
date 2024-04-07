/*
* Copyright (c) 2024 EKA2L1 Team.
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
   class camera_pan: public base {
   private:
       friend class editor;

       drivers::handle pan_icon_;
       float pan_speed_;

   public:
       explicit camera_pan(editor *editor_instance, const eka2l1::vec2 &position);

       void draw(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder,
           const eka2l1::vec2f &scale_factor) override;

       editor_entity_type entity_type() const override {
           return EDITOR_ENTITY_TYPE_CAMERA_PAN;
       }

       action_result on_mouse_action(const eka2l1::vec2 &value, const int action, const bool is_selected) override;

       void on_key_press(const std::uint32_t code) override {

       }

       void on_controller_button_press(const std::uint32_t code) override {

       }

       bool in_content_area(const eka2l1::vec2 &pos) const override;

       void update_key_wait_statuses(const bool selected) {

       }

       float pan_speed() const {
           return pan_speed_;
       }

        void pan_speed(const float new_speed) {
            pan_speed_ = new_speed;
        }
   };
}
