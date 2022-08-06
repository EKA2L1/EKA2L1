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

#include <qt/btnmap/singletouch.h>
#include <qt/btnmap/editor.h>

#include <drivers/input/common.h>
#include <QKeyEvent>

namespace eka2l1::qt::btnmap {
    single_touch::single_touch(editor *editor_instance, const eka2l1::vec2 &pos)
        : base(editor_instance, pos)
        , keycode_('A') 
        , waiting_key_(false) {

    }

    static constexpr std::uint32_t FIELD_TEXT_PADDING = editor::DEFAULT_OVERLAY_FONT_SIZE / 14 * 2;
    static constexpr std::uint32_t FIELD_TEXT_HEIGHT = editor::DEFAULT_OVERLAY_FONT_SIZE + FIELD_TEXT_PADDING * 2;
    static const eka2l1::vec3 GRAY_COLOUR = eka2l1::vec3(0xF2, 0xF2, 0xF2);
    static const eka2l1::vec3 BLACK_COLOUR = eka2l1::vec3(0, 0, 0);
    
    eka2l1::rect single_touch::get_content_box(const std::string &draw_str) const {
        // Taking the position as the center
        eka2l1::rect draw_box;
        draw_box.size = eka2l1::vec2(static_cast<int>(editor::DEFAULT_OVERLAY_FONT_SIZE * draw_str.length()
            + FIELD_TEXT_PADDING * 2), FIELD_TEXT_HEIGHT);
        draw_box.top = position_ - draw_box.size / 2;
        draw_box.scale(scale_factor_);
    
        return draw_box;
    }

    const std::string single_touch::get_readable_keyname() const {
        if (waiting_key_) {
            return "_";
        }

        if (type_ == MAP_TYPE_KEYBOARD) {
            return QKeySequence(keycode_).toString().toStdString();
        } else {
            return std::string("GP ") + drivers::button_to_string(static_cast<drivers::controller_button_code>(keycode_));
        }
    }

    void single_touch::draw(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder,
        const float scale_factor) {
        base::draw(driver, builder, scale_factor);
        std::string draw_str = get_readable_keyname();
        eka2l1::rect draw_box = get_content_box(draw_str);

        builder.set_brush_color(GRAY_COLOUR);
        builder.draw_rectangle(draw_box);

        draw_box.top.y += (draw_box.size.y - FIELD_TEXT_PADDING * scale_factor);

        builder.set_brush_color(BLACK_COLOUR);
        editor_->draw_text(driver, builder, draw_box, epoc::text_alignment::center, draw_str, scale_factor);
    }

    bool single_touch::in_content_area(const eka2l1::vec2 &pos) const {
        std::string draw_str = get_readable_keyname();
        eka2l1::rect draw_box = get_content_box(draw_str);

        return draw_box.contains(pos);
    }

    action_result single_touch::on_mouse_action(const eka2l1::vec2 &value, const int action, const bool selected) {
        const action_result res = base::on_mouse_action(value, action, selected);
        if ((action == 2) && selected && !dragging_) {
            waiting_key_ = true;
        } else if ((res == ACTION_DESELECTED) || dragging_) {
            waiting_key_ = false;
        }

        return res;
    }

    void single_touch::update_key_wait_statuses(const bool selected) {
        if (waiting_key_ && (!selected || dragging_)) {
            waiting_key_ = false;
        }
    }

    void single_touch::on_key_press(const std::uint32_t code) {
        if (waiting_key_) {
            type_ = MAP_TYPE_KEYBOARD;
            keycode_ = code;

            waiting_key_ = false;
        }
    }

    void single_touch::on_controller_button_press(const std::uint32_t code) {
        if (waiting_key_) {
            type_ = MAP_TYPE_GAMEPAD;
            keycode_ = code;

            waiting_key_ = false;
        }
    }
}
