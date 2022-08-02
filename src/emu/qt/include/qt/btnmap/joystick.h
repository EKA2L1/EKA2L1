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
