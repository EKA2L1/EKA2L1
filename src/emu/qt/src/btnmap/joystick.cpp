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

#include <qt/btnmap/joystick.h>
#include <qt/btnmap/editor.h>
#include <qt/btnmap/executor.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <common/fileutils.h>
#include <common/log.h>

#include <QApplication>

namespace eka2l1::qt::btnmap {
    static constexpr std::uint32_t MIN_JOYSTICK_SIZE = 36;
    static const eka2l1::vec2 CORNER_RESIZE_SIZE = eka2l1::vec2(5, 5);

    joystick::joystick(editor *editor_instance, const eka2l1::vec2 &position)
        : base(editor_instance, position)
        , base_tex_handle_(0)
        , width_(0)
        , focusing_key_(-1)
        , resizing_mode_(-1) {
        for (std::size_t i = 0; i < 5; i++) {
            keys_[i] = nullptr;
        }

        // Width should be equals to height :)
        if (width_ == 0) {
            width_ = 100;
        }

        positioning_bind_buttons();
    }

    void joystick::positioning_bind_buttons() {
        int padding = static_cast<int>(+ common::min<int>(width_ / 10, 10));

        eka2l1::vec2 centers[5] = {
            eka2l1::vec2(position_.x + width_ / 2, position_.y + padding),     // Up
            eka2l1::vec2(position_.x + width_ / 2, position_.y + width_ - padding),    // Down
            eka2l1::vec2(position_.x + padding, position_.y + width_ / 2),     // Left
            eka2l1::vec2(position_.x + width_ - padding, position_.y + width_ / 2),   // Right
            position_ + eka2l1::vec2(width_ / 2)    // Joy
        };

        for (std::size_t i = 0; i < 4; i++) {
            static const char *JOY_WASD = "WSAD";

            if (!keys_[i]) {
                keys_[i] = std::make_unique<single_touch>(editor_, centers[i]);
                keys_[i]->type(MAP_TYPE_KEYBOARD);
                keys_[i]->keycode(JOY_WASD[i]);
            } else {
                keys_[i]->position(centers[i]);
            }
        }

        if (!keys_[4]) {
            keys_[4] = std::make_unique<single_touch>(editor_, centers[4]);
            keys_[4]->type(MAP_TYPE_GAMEPAD);
            keys_[4]->keycode(eka2l1::drivers::CONTROLLER_BUTTON_CODE_LEFT_STICK);
        } else {
            keys_[4]->position(centers[4]);
        }
    }

    void joystick::draw(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder,
        const eka2l1::vec2f &scale_vector) {
        base::draw(driver, builder, scale_vector);
        static const char *JOYSTICK_BASE_PNG_PATH = "resources\\joystick_base.png";

        if (!base_tex_handle_) {
            static const char *JOYSTICK_BASE_TEX_RESOURCE_NAME = "joystick_base_texture";
            base_tex_handle_ = editor_->get_managed_resource(driver, builder, JOYSTICK_BASE_TEX_RESOURCE_NAME,
                JOYSTICK_BASE_PNG_PATH);
        }

        eka2l1::rect dest_rect;
        dest_rect.top = position_;
        dest_rect.size = eka2l1::vec2(width_);

        dest_rect.scale(scale_vector);

        builder.set_brush_color_detail(eka2l1::vec4(255, 255, 255, 255));
        builder.set_feature(eka2l1::drivers::graphics_feature::blend, true);

        builder.draw_bitmap(base_tex_handle_, 0, dest_rect, eka2l1::rect());

        if (editor_->selected() == this) {
            builder.set_pen_style(drivers::pen_style_solid);

            builder.draw_line(dest_rect.top, dest_rect.top + eka2l1::vec2(dest_rect.size.x, 0));
            builder.draw_line(dest_rect.top + eka2l1::vec2(dest_rect.size.x, 0), dest_rect.top + dest_rect.size);
            builder.draw_line(dest_rect.top + dest_rect.size, dest_rect.top + eka2l1::vec2(0, dest_rect.size.y));
            builder.draw_line(dest_rect.top + eka2l1::vec2(0, dest_rect.size.y), dest_rect.top);

            eka2l1::rect corner_rect;
            corner_rect.top = position_;
            corner_rect.size = CORNER_RESIZE_SIZE;

            corner_rect.scale(scale_factor_);
            builder.draw_rectangle(corner_rect);

            corner_rect.top = (position_ + eka2l1::vec2(width_ - CORNER_RESIZE_SIZE.x, 0)) * scale_factor_;
            builder.draw_rectangle(corner_rect);
            
            corner_rect.top = (position_ + width_ - CORNER_RESIZE_SIZE) * scale_factor_;
            builder.draw_rectangle(corner_rect);

            corner_rect.top = (position_ + eka2l1::vec2(0, width_ - CORNER_RESIZE_SIZE.x)) * scale_factor_;
            builder.draw_rectangle(corner_rect);
        }

        for (std::size_t i = 0; i < 5; i++) {
            if (keys_[i]) {
                keys_[i]->draw(driver, builder, scale_vector);
            }
        }
    }

    bool joystick::in_content_area(const eka2l1::vec2 &pos) const {
        eka2l1::rect draw_box = eka2l1::rect(position_, eka2l1::vec2(width_));
        draw_box.scale(scale_factor_);

        return draw_box.contains(pos);
    }

    void joystick::on_dragging(const eka2l1::vec2 &value) {
        if (resizing_mode_ == -1) {
            // Upper corner
            eka2l1::rect corner;
            corner.size = CORNER_RESIZE_SIZE;
            corner.top = position_;
            corner.scale(scale_factor_);

            if (corner.contains(start_dragging_pos_)) {
                resizing_mode_ = 0;
            } else {
                // Right-upper corner
                corner.top = (position_ + eka2l1::vec2(width_ - CORNER_RESIZE_SIZE.x, 0)) * scale_factor_;
                if (corner.contains(start_dragging_pos_)) {
                    resizing_mode_ = 1;
                } else {
                    // Left-bottom corner
                    corner.top = (position_ + eka2l1::vec2(0, width_ - CORNER_RESIZE_SIZE.x)) * scale_factor_;
                    if (corner.contains(start_dragging_pos_)) {
                        resizing_mode_ = 2;
                    } else {
                        corner.top = (position_ + eka2l1::vec2(width_) - CORNER_RESIZE_SIZE) * scale_factor_;
                        if (corner.contains(start_dragging_pos_)) {
                            resizing_mode_ = 3;
                        }
                    }
                }
            }
        }

        switch (resizing_mode_) {
        case 0:
            if (value <= ((previous_rect_.top + previous_rect_.size) * scale_factor_)) {
                if (common::abs(previous_rect_.top.x - value.x) >= common::abs(previous_rect_.top.y - value.y)) {
                    width_ = previous_rect_.top.y + previous_rect_.size.y - static_cast<int>(value.y / scale_factor_[1]);
                } else {
                    width_ = previous_rect_.top.x + previous_rect_.size.x - static_cast<int>(value.x / scale_factor_[0]);
                }

                width_ = common::max(width_, MIN_JOYSTICK_SIZE);
                position_ = previous_rect_.top + previous_rect_.size - eka2l1::vec2(width_);
            } else {
                if (common::abs(previous_rect_.top.x - value.x) >= common::abs(previous_rect_.top.y - value.y)) {
                    width_ = static_cast<int>(value.y / scale_factor_[1]) - previous_rect_.top.y;
                } else {
                    width_ = static_cast<int>(value.x / scale_factor_[0]) - previous_rect_.top.x;
                }

                width_ = common::max(width_, MIN_JOYSTICK_SIZE);
                position_ = previous_rect_.top + previous_rect_.size;
            }

            break;

        case 1:
            if (value.x >= static_cast<int>(previous_rect_.top.x * scale_factor_[0])) {
                width_ = static_cast<int>(value.x / scale_factor_[0]) - previous_rect_.top.x;
                width_ = common::max(width_, MIN_JOYSTICK_SIZE);
            } else {
                width_ =  previous_rect_.top.x - static_cast<int>(value.x / scale_factor_[0]);
                width_ = common::max(width_, MIN_JOYSTICK_SIZE);
                position_ = previous_rect_.top - eka2l1::vec2(width_, 0);
            }
            break;

        case 2:
            if (value.y >= static_cast<int>(previous_rect_.top.y * scale_factor_[1])) {
                width_ = static_cast<int>(value.y / scale_factor_[1]) - previous_rect_.top.y;
                width_ = common::max(width_, MIN_JOYSTICK_SIZE);
            } else {
                width_ = previous_rect_.top.y - static_cast<int>(value.y / scale_factor_[1]);
                width_ = common::max(width_, MIN_JOYSTICK_SIZE);
                position_ = previous_rect_.top - eka2l1::vec2(0, width_);
            }
            break;

        case 3:
            if (value >= (previous_rect_.top * scale_factor_)) {
                if (common::abs(previous_rect_.top.x + previous_rect_.size.x - value.x) >= common::abs(previous_rect_.top.y + previous_rect_.size.y - value.y)) {
                    width_ = static_cast<int>(value.y / scale_factor_[1]) - position_.y;
                } else {
                    width_ = static_cast<int>(value.x / scale_factor_[0]) - position_.x;
                }

                width_ = common::max(width_, MIN_JOYSTICK_SIZE);
            } else {
                if (common::abs(previous_rect_.top.x - value.x) >= common::abs(previous_rect_.top.y - value.y)) {
                    width_ = previous_rect_.top.y - static_cast<int>(value.y / scale_factor_[1]);
                } else {
                    width_ = previous_rect_.top.x - static_cast<int>(value.x / scale_factor_[0]);
                }

                position_ = previous_rect_.top - eka2l1::vec2(width_);
            }

            break;

        default:
            base::on_dragging(value);
            break;
        }

        positioning_bind_buttons();
    }

    action_result joystick::on_mouse_action(const eka2l1::vec2 &value, const int action, const bool selected) {
        if ((action == 2) && !dragging_ && selected) {
            for (int i = 0; i < 5; i++) {
                action_result res = keys_[i]->on_mouse_action(start_dragging_pos_, 0, (focusing_key_ == i));
                if (res != ACTION_NOTHING) {
                    if (res == ACTION_SELECTED) {
                        focusing_key_ = i;
                    } else {
                        if (focusing_key_ == i) {
                            focusing_key_ = -1;
                        }
                    }
                }
            }

            for (int i = 0; i < 5; i++) {
                keys_[i]->update_key_wait_statuses((focusing_key_ == i));
            }

            for (int i = 0; i < 5; i++) {
                keys_[i]->on_mouse_action(start_dragging_pos_, 2, (focusing_key_ == i));
            }

            if (focusing_key_ != -1) {
                return ACTION_NOTHING;
            }
        }

        const action_result res = base::on_mouse_action(value, action, selected);
        if ((action == 0) && (res != ACTION_DESELECTED)) {
            previous_rect_.top = position_;
            previous_rect_.size = eka2l1::vec2(width_);

            resizing_mode_ = -1;
        }

        return res;
    }

    void joystick::on_key_press(const std::uint32_t code) {
        if ((focusing_key_ < 4) && (focusing_key_ >= 0)) {
            keys_[focusing_key_]->on_key_press(code);
        }
    }

    void joystick::on_controller_button_press(const std::uint32_t code) {
        if ((focusing_key_ < 4) && (focusing_key_ >= 0)) {
            keys_[focusing_key_]->on_controller_button_press(code);
        }
    }

    void joystick::on_joy_move(const std::uint32_t code, const float axisx, const float axisy) {
        if (focusing_key_ == 4) {
            if ((std::abs(axisx) >= JOY_DEADZONE_VALUE) || (std::abs(axisy) >= JOY_DEADZONE_VALUE)) {
                keys_[focusing_key_]->on_controller_button_press(code);
            }
        }
    }
}
