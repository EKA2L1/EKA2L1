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
