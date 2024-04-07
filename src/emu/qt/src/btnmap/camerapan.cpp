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

#include <qt/btnmap/camerapan.h>
#include <qt/btnmap/editor.h>

#include <drivers/input/common.h>
#include <QKeyEvent>
#include <QObject>
#include <QInputDialog>

namespace eka2l1::qt::btnmap {
   camera_pan::camera_pan(editor *editor_instance, const eka2l1::vec2 &pos)
        : base(editor_instance, pos)
        , pan_icon_(0)
        , pan_speed_(4.0f) {

   }

   static constexpr std::uint32_t FIELD_TEXT_PADDING = editor::DEFAULT_OVERLAY_FONT_SIZE / 14 * 2;
   static constexpr std::uint32_t FIELD_TEXT_HEIGHT = editor::DEFAULT_OVERLAY_FONT_SIZE + FIELD_TEXT_PADDING * 2;
   static const eka2l1::vec3 GRAY_COLOUR = eka2l1::vec3(0xF2, 0xF2, 0xF2);
   static const eka2l1::vec3 BLACK_COLOUR = eka2l1::vec3(0, 0, 0);

   static constexpr std::uint32_t PAN_ICON_SIZE = editor::DEFAULT_OVERLAY_FONT_SIZE * 2;

   action_result camera_pan::on_mouse_action(const eka2l1::vec2 &value, const int action, const bool is_selected) {
       const action_result res = base::on_mouse_action(value, action, is_selected);
       if ((action == 2)  && is_selected && !dragging_) {
           bool is_ok = false;
           auto result = QInputDialog::getDouble(nullptr, "Set panning speed", "Panning speed", pan_speed_, 1.0f, 20.0f, 2, &is_ok, Qt::WindowFlags(),
               1);

           if (is_ok) {
               pan_speed_ = static_cast<float>(result);
           }
       }

       return res;
   }

   void camera_pan::draw(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder,
       const eka2l1::vec2f &scale_factor) {
       base::draw(driver, builder, scale_factor);
       std::string draw_str = std::string("Mouse/GP ") +
           drivers::button_to_string(drivers::controller_button_code::CONTROLLER_BUTTON_CODE_RIGHT_STICK);

       if (!pan_icon_) {
           pan_icon_ = editor_->get_managed_resource(driver, builder, "pan_icon", "resources/camera_panning.png");
       }

       eka2l1::rect draw_box;
       draw_box.size = eka2l1::vec2(PAN_ICON_SIZE, PAN_ICON_SIZE);
       draw_box.top = position_ - draw_box.size / 2;
       draw_box.scale(scale_factor_);

       builder.draw_bitmap(pan_icon_, 0, draw_box, eka2l1::rect{});

       draw_box.size = eka2l1::vec2(static_cast<int>(editor::DEFAULT_OVERLAY_FONT_SIZE * draw_str.length()
                                        + FIELD_TEXT_PADDING * 2), FIELD_TEXT_HEIGHT);
       draw_box.top = position_ + eka2l1::vec2(0, PAN_ICON_SIZE * 3 / 2) - draw_box.size / 2;
       draw_box.scale(scale_factor_);

       builder.set_brush_color(GRAY_COLOUR);
       builder.draw_rectangle(draw_box);

       draw_box.top.y += (draw_box.size.y - FIELD_TEXT_PADDING * scale_factor[1]);

       builder.set_brush_color(BLACK_COLOUR);
       editor_->draw_text(driver, builder, draw_box, epoc::text_alignment::center, draw_str, scale_factor);
   }

   bool camera_pan::in_content_area(const eka2l1::vec2 &pos) const {
       eka2l1::rect draw_box_icon;
       draw_box_icon.size = eka2l1::vec2(PAN_ICON_SIZE, PAN_ICON_SIZE);
       draw_box_icon.top = position_ - draw_box_icon.size / 2;
       draw_box_icon.scale(scale_factor_);

       if (draw_box_icon.contains(pos)) {
            return true;
       }

       std::string draw_str = std::string("Mouse/GP ") +
           drivers::button_to_string(drivers::controller_button_code::CONTROLLER_BUTTON_CODE_RIGHT_STICK);

       eka2l1::rect draw_box;

       draw_box.size = eka2l1::vec2(static_cast<int>(editor::DEFAULT_OVERLAY_FONT_SIZE * draw_str.length() + FIELD_TEXT_PADDING * 2), FIELD_TEXT_HEIGHT) +
           eka2l1::vec2(0, PAN_ICON_SIZE * 3 / 2);

       draw_box.top = position_ - draw_box.size / 2;

       draw_box.scale(scale_factor_);

       return draw_box.contains(pos);
   }
}
