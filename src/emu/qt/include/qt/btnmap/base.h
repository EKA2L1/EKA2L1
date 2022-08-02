#pragma once

#include <common/vecx.h>
#include <drivers/itc.h>
#include <memory>

namespace eka2l1::qt::btnmap {
    class editor;

    enum map_type {
        MAP_TYPE_KEYBOARD = 0,
        MAP_TYPE_GAMEPAD = 1
    };

    enum action_result {
        ACTION_NOTHING = 0,
        ACTION_SELECTED = 1,
        ACTION_DESELECTED = 2
    };

    enum editor_entity_type {
        EDITOR_ENTITY_TYPE_JOYSTICK = 0,
        EDITOR_ENTITY_TYPE_SINGLE_TOUCH = 1,
        EDITOR_ENTITY_TYPE_AIM = 2
    };

    class base {
    protected:
        editor *editor_;
        eka2l1::vec2 position_;
        eka2l1::vec2 diff_origin_;
        map_type type_;
        float scale_factor_;

        eka2l1::vec2 start_dragging_pos_;
        bool dragging_;

        virtual bool in_content_area(const eka2l1::vec2 &pos) const = 0;
        virtual void on_dragging(const eka2l1::vec2 &current_pos);

    public:
        explicit base(editor *editor_instace, const eka2l1::vec2 &position = eka2l1::vec2(0, 0));

        virtual void draw(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder,
            const float scale_factor) {
            scale_factor_ = scale_factor;
        }

        const eka2l1::vec2 &position() const {
            return position_;
        }

        void position(const eka2l1::vec2 &new_position) {
            position_ = new_position;
        }

        virtual action_result on_mouse_action(const eka2l1::vec2 &value, const int action, const bool is_selected);
        virtual void on_key_press(const std::uint32_t code) = 0;
        virtual void on_controller_button_press(const std::uint32_t code) = 0;
        virtual void on_joy_move(const std::uint32_t code, const float axisx, const float axisy) {}

        const map_type type() const {
            return type_;
        }

        void type(const map_type type) {
            type_ = type;
        }

        virtual editor_entity_type entity_type() const = 0;
    };

    using base_instance = std::unique_ptr<base>;
}
