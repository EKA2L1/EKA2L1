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

#include <QDockWidget>
#include <QString>

#include <qt/btnmap/editor.h>
#include <drivers/input/common.h>

namespace Ui {
    class editor_widget;
}

namespace eka2l1 {
    namespace qt::btmap {
        struct executor;
    }
    class system;
}

class editor_widget : public QDockWidget {
    Q_OBJECT

private:
    Ui::editor_widget *ui_;
    eka2l1::qt::btnmap::editor map_editor_;
    bool map_editor_hidden_;
    float last_scale_factor_;
    eka2l1::system *system_;

    std::string current_app_uid_;
    std::string current_app_name_;

    QString profile_folder_path_;
    QString profile_use_config_file_;
    QWidget *empty_widget_;

    eka2l1::qt::btnmap::executor *map_exec_;
    int previous_profile_combo_index_;

private slots:
    void on_edit_btn_clicked(bool checked);
    void on_delete_btn_clicked(bool checked);
    void on_potential_current_app_changed();
    void on_profile_add_btn_clicked();
    void on_profile_delete_btn_clicked();
    void on_profile_combo_box_activated(int index);
    void on_save_toggle_profile_btn_clicked();

signals:
    void potential_current_app_changed();
    void editor_hidden();

public:
    explicit editor_widget(QWidget *parent, eka2l1::qt::btnmap::executor *exec);
    ~editor_widget();

    void hear_active_app(eka2l1::system *sys);
    void draw(eka2l1::drivers::graphics_driver *driver, eka2l1::drivers::graphics_command_builder &builder,
              const float scale_factor);

    bool handle_key_press(const std::uint32_t code);
    bool handle_mouse_action(const eka2l1::vec3 &pos, const int button_id, const int action_id,
                             const int mouse_id);
    bool handle_controller_event(const eka2l1::drivers::input_event &evt);
};
