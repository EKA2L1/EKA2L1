#pragma once

#include <services/fbs/adapter/font_adapter.h>
#include <services/fbs/font_atlas.h>
#include <qt/btnmap/base.h>

#include <drivers/itc.h>
#include <common/vecx.h>

#include <mutex>
#include <vector>

#include <QMouseEvent>

namespace eka2l1::qt::btnmap {
    struct executor;

    class editor {
    public:
        static constexpr std::uint32_t DEFAULT_OVERLAY_FONT_SIZE = 14;

    private:
        epoc::adapter::font_file_adapter_instance adapter_;
        std::unique_ptr<epoc::font_atlas> atlas_;

        std::vector<base_instance> entities_;
        std::mutex lock_;

        std::map<std::string, drivers::handle> resources_;
        base *selected_entity_;

    public:
        explicit editor();

        void clean(drivers::graphics_driver *driver);
        void add_managed_resource(const std::string &name, drivers::handle h);
        drivers::handle get_managed_resource(const std::string &name);

        void draw_text(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder,
                       const eka2l1::rect &draw_rect, epoc::text_alignment alignment,
                       const std::string &str, const float scale_factor);

        void draw(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder,
                  const float scale_factor);

        void on_mouse_event(const eka2l1::vec3 &pos, const int button_id, const int action_id,
            const int mouse_id);

        void on_key_press(const std::uint32_t code);
        void on_controller_button_press(const std::uint32_t code);
        void on_joy_move(const std::uint32_t code, const float axisx, const float axisy);

        void add_and_select_entity(base_instance &instance);
        void delete_entity(base *b);

        void import_from(executor *exec);
        void export_to(executor *exec);

        base *selected() const {
            return selected_entity_;
        }
    };
}
