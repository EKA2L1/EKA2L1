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

#include <qt/btnmap/base.h>
#include <qt/btnmap/editor.h>

#include <QApplication>

namespace eka2l1::qt::btnmap {
    base::base(editor *editor_instace, const eka2l1::vec2 &position)
        : editor_(editor_instace)
        , position_(position)
        , type_(MAP_TYPE_KEYBOARD)
        , dragging_(false) {

    }

    void base::on_dragging(const eka2l1::vec2 &current_pos) {
        position_ = (current_pos - diff_origin_) / scale_factor_;
    }
    
    action_result base::on_mouse_action(const eka2l1::vec2 &value, const int action, const bool selected) {
        if (action == 0) {
            dragging_ = false;

            if (in_content_area(value)) {
                start_dragging_pos_ = value;

                if (!selected) {
                    return ACTION_SELECTED;
                } else {
                    return ACTION_NOTHING;
                }
            }

            return ACTION_DESELECTED;
        } else if (action == 2) {
            return ACTION_NOTHING;
        }

        if (editor_->selected() != this) {
            return ACTION_NOTHING;
        }

        // Drag
        if (!dragging_) {
            if (static_cast<int>((value - start_dragging_pos_).length()) < QApplication::startDragDistance()) {
                return ACTION_NOTHING;
            }

            dragging_ = true;
            diff_origin_ = (start_dragging_pos_ - position_ * scale_factor_);
        }

        on_dragging(value);
        return ACTION_NOTHING;
    }
}
