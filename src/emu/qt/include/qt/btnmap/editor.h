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
                       const std::string &str, const eka2l1::vec2f &scale_factor);

        void draw(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder,
                  const eka2l1::vec2f &scale_factor);

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
