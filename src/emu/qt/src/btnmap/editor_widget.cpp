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

#include "ui_editor_widget.h"
#include <qt/btnmap/executor.h>
#include <qt/btnmap/editor_widget.h>
#include <qt/btnmap/singletouch.h>
#include <qt/btnmap/joystick.h>
#include <qt/utils.h>

#include <common/cvt.h>
#include <QDir>
#include <QLabel>
#include <QGridLayout>
#include <QInputDialog>

editor_widget::editor_widget(QWidget *parent, eka2l1::qt::btnmap::executor *exec)
    : QDockWidget(parent)
    , ui_(new Ui::editor_widget)
    , map_editor_hidden_(false)
    , last_scale_factor_({ 1.0f, 1.0f })
    , system_(nullptr)
    , map_exec_(exec)
    , previous_profile_combo_index_(-1){
    ui_->setupUi(this);
    ui_->edit_mode_btn->setChecked(true);
    ui_->map_comps_list->setCurrentRow(0);

    setFocusPolicy(Qt::NoFocus);

    empty_widget_ = new QWidget;

    QGridLayout *empty_layout = new QGridLayout;
    QLabel *label = new QLabel(tr("Please wait for a game to be active!"));
    empty_layout->setAlignment(Qt::AlignCenter);
    empty_layout->addWidget(label);
    empty_widget_->setLayout(empty_layout);

    setWidget(empty_widget_);

    connect(ui_->edit_mode_btn, &QPushButton::clicked, this, &editor_widget::on_edit_btn_clicked);
    connect(ui_->delete_mode_btn, &QPushButton::clicked, this, &editor_widget::on_delete_btn_clicked);
    connect(ui_->profile_combo_box, QOverload<int>::of(&QComboBox::activated), this, &editor_widget::on_profile_combo_box_activated);
    connect(this, &editor_widget::potential_current_app_changed, this, &editor_widget::on_potential_current_app_changed);
}

editor_widget::~editor_widget() {
    setWidget(ui_->editor_widget_avail_content);

    delete empty_widget_;
    delete ui_;
}

void editor_widget::hear_active_app(eka2l1::system *sys) {
    system_ = sys;
    eka2l1::window_server *winserv = ::get_window_server_through_system(sys);
    if (winserv) {
        eka2l1::epoc::screen *scr = winserv->get_screens();
        scr->add_focus_change_callback(nullptr, [this](void *userdata, eka2l1::epoc::window_group *, eka2l1::epoc::focus_change_property) {
            emit potential_current_app_changed();
        });
    }
}

static const char *BINDING_TOUCH_FOLDER_FORMAT = "bindings/touch/%1";

void editor_widget::on_potential_current_app_changed() {
    std::optional<eka2l1::akn_running_app_info> current_app_info = ::get_active_app_info(system_);
    if (current_app_info) {
        current_app_uid_ = fmt::format("{:X}", current_app_info->app_uid_);
        current_app_name_ = eka2l1::common::ucs2_to_utf8(current_app_info->app_name_);

        ui_->current_app_profile_label->setText(tr("For app: %1 (0x%2)").arg(QString::fromStdString(current_app_name_),
                                                                             QString::fromStdString(current_app_uid_)));

        profile_folder_path_ = QString(BINDING_TOUCH_FOLDER_FORMAT).arg(QString::fromStdString(current_app_uid_));
        
        ui_->profile_combo_box->clear();

        QDir dir(profile_folder_path_);
        if (!dir.exists()) {
            // Well, no binding at the moment :D
            QDir().mkpath(dir.path());
        } else {
            QStringList filters;
            filters << "*.yml" << "*.json";

            dir.setFilter(QDir::Files | QDir::NoSymLinks);
            dir.setNameFilters(filters);

            QFileInfoList list = dir.entryInfoList();

            for (int i = 0; i < list.size(); i++) {
                QFileInfo &info = list[i];
                ui_->profile_combo_box->addItem(info.baseName());
            }
        }

        if (ui_->profile_combo_box->count() == 0) {
            ui_->profile_combo_box->addItem("default");
        }

        profile_use_config_file_ = profile_folder_path_ + "/Game name - " + QString::fromStdString(current_app_name_);

        std::uint32_t current_index = 0;

        QFile file(profile_use_config_file_);
        if (!file.exists()) {
            file.open(QIODevice::WriteOnly | QIODevice::NewOnly);
            file.write(reinterpret_cast<const char*>(&current_index), sizeof(current_index));
        } else {
            if (file.open(QIODevice::ReadOnly)) {
                file.read(reinterpret_cast<char*>(&current_index), sizeof(current_index));
            }
        }

        file.close();

        if (ui_->profile_combo_box->count() >= 1) {
            const int index_choose = eka2l1::common::min<int>(ui_->profile_combo_box->count() - 1, static_cast<int>(current_index));

            ui_->profile_combo_box->setCurrentIndex(index_choose);
            on_profile_combo_box_activated(index_choose);
        }

        setWidget(ui_->editor_widget_avail_content);
    } else {
        current_app_uid_.clear();
        setWidget(empty_widget_);
    }
}

void editor_widget::on_profile_add_btn_clicked() {
    bool is_ok = false;
    QString result = QInputDialog::getText(this, tr("Enter profile name"), QString(), QLineEdit::Normal, QString(), &is_ok);

    if (!is_ok) {
        return;
    }

    QString profile_path_name = (profile_folder_path_ + "/" + result + ".yml");
    if (!QDir().exists(profile_folder_path_)) {
        QDir().mkpath(profile_folder_path_);
    }

    QFile file(profile_path_name);
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        QMessageBox::critical(this, tr("Error creating profile!"), tr("Profile name contains special characters or system error encountered!"));
    } else {
        file.close();

        ui_->profile_combo_box->addItem(result);
        ui_->profile_combo_box->setCurrentIndex(ui_->profile_combo_box->count() - 1);
    }
}

void editor_widget::on_profile_delete_btn_clicked() {
    QString profile_path_name = (profile_folder_path_ + "/" + ui_->profile_combo_box->currentText() + ".yml");
    QDir dir;

    if (previous_profile_combo_index_ == ui_->profile_combo_box->currentIndex()) {
        previous_profile_combo_index_ = -1;
    }

    dir.remove(profile_path_name);
    ui_->profile_combo_box->removeItem(ui_->profile_combo_box->currentIndex());
}

void editor_widget::on_edit_btn_clicked(bool checked) {
    if (checked) {
        ui_->delete_mode_btn->setChecked(false);
    }
}

void editor_widget::on_delete_btn_clicked(bool checked) {
    if (checked) {
        ui_->edit_mode_btn->setChecked(false);
    }
}

void editor_widget::draw(eka2l1::drivers::graphics_driver *driver, eka2l1::drivers::graphics_command_builder &builder,
    const eka2l1::vec2f &scale_factor) {
    last_scale_factor_ = scale_factor;

    if (!map_editor_hidden_) {
        map_editor_.draw(driver, builder, scale_factor);
    }
}

bool editor_widget::handle_key_press(const std::uint32_t code)  {
    if (map_editor_hidden_) {
        return false;
    }

    map_editor_.on_key_press(code);
    return true;
}

bool editor_widget::handle_controller_event(const eka2l1::drivers::input_event &evt) {
    if (map_editor_hidden_) {
        return false;
    }

    if (evt.button_.state_ == eka2l1::drivers::button_state::joy) {
        map_editor_.on_joy_move(evt.button_.button_, evt.button_.axis_[0], evt.button_.axis_[1]);
    } else if (evt.button_.state_ == eka2l1::drivers::button_state::pressed) {
        map_editor_.on_controller_button_press(evt.button_.button_);
    }

    return true;
}

bool editor_widget::handle_mouse_action(const eka2l1::vec3 &pos, const int button_id, const int action_id,
    const int mouse_id) {
    if (map_editor_hidden_) {
        return false;
    }

    map_editor_.on_mouse_event(pos, button_id, action_id, mouse_id);

    if ((action_id == 0) && !map_editor_.selected()) {
        if (ui_->edit_mode_btn->isChecked()) {
            int row = ui_->map_comps_list->currentRow();
            eka2l1::qt::btnmap::base_instance inst = nullptr;
            eka2l1::vec2 pos_place = eka2l1::vec2(pos.x, pos.y) / last_scale_factor_;
            switch (row) {
            case 0:
                inst = std::make_unique<eka2l1::qt::btnmap::joystick>(&map_editor_, pos_place);
                break;

            case 1:
                inst = std::make_unique<eka2l1::qt::btnmap::single_touch>(&map_editor_, pos_place);
                break;

            default:
                break;
            }

            if (inst) {
                map_editor_.add_and_select_entity(inst);
            }
        }
    } else if (map_editor_.selected() && (action_id == 2)) {
        if (ui_->delete_mode_btn->isChecked()) {
            map_editor_.delete_entity(map_editor_.selected());
        }
    }
    return true;
}

void editor_widget::on_save_toggle_profile_btn_clicked() {
    if (map_editor_hidden_) {
        ui_->edit_mode_btn->setDisabled(false);
        ui_->delete_mode_btn->setDisabled(false);
        ui_->profile_add_btn->setDisabled(false);
        ui_->profile_remove_btn->setDisabled(false);

        ui_->save_toggle_profile_btn->setText(tr("Save and hide editor"));
    } else {
        QString previous_profile_path_name = (profile_folder_path_ + "/" + ui_->profile_combo_box->currentText() + ".yml");
        map_editor_.export_to(map_exec_);
        map_exec_->serialize(previous_profile_path_name.toStdString());

        ui_->edit_mode_btn->setDisabled(true);
        ui_->delete_mode_btn->setDisabled(true);
        ui_->profile_add_btn->setDisabled(true);
        ui_->profile_remove_btn->setDisabled(true);

        ui_->save_toggle_profile_btn->setText(tr("Show editor"));

        emit editor_hidden();
    }

    map_editor_hidden_ = !map_editor_hidden_;
}

void editor_widget::on_profile_combo_box_activated(int index) {
    if (previous_profile_combo_index_ >= 0) {
        QString previous_profile_path_name = (profile_folder_path_ + "/" + ui_->profile_combo_box->itemText(previous_profile_combo_index_) + ".yml");
        map_editor_.export_to(map_exec_);
        map_exec_->serialize(previous_profile_path_name.toStdString());
    }

    QString profile_path_name = (profile_folder_path_ + "/" + ui_->profile_combo_box->itemText(index) + ".yml");
    map_exec_->deserialize(profile_path_name.toStdString());
    map_editor_.import_from(map_exec_);

    QFile file(profile_use_config_file_);
    file.open(QIODevice::WriteOnly);
    file.write(reinterpret_cast<const char*>(&index), sizeof(index));

    previous_profile_combo_index_ = index;
}
