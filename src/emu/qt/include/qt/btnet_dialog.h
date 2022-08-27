/*
 * Copyright (c) 2022 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project
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

#include <QDialog>

namespace Ui {
    class btnet_dialog;
}

namespace eka2l1::config {
    struct state;
}

class btnet_dialog : public QDialog {
    Q_OBJECT
private:
    Ui::btnet_dialog *ui_;
    eka2l1::config::state &conf_;

    bool modded_;
    bool modded_once_;

    void show_opts_accord_to_index(int index);

private slots:
    void on_mode_combo_box_activated(int index);
    void on_direct_ip_editor_open_btn_clicked();
    void on_enable_upnp_checkbox_clicked(bool toggled);
    void on_save_btn_clicked();
    void on_password_line_edit_textEdited(const QString &text);
    void on_server_addr_edit_line_textEdited(const QString &text);
    void on_password_clear_btn_clicked();
    void on_password_gen_random_btn_clicked();

signals:
    void direct_ip_editor_requested();

public:
    explicit btnet_dialog(QWidget *parent, eka2l1::config::state &conf);
    ~btnet_dialog();

    void closeEvent(QCloseEvent *evt) override;
};
