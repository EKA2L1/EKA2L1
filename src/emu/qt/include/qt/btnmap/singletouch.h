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
