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

#include "ui_btnet_dialog.h"
#include <qt/btnet_dialog.h>
#include <config/config.h>
#include <common/random.h>

#include <common/algorithm.h>
#include <services/bluetooth/protocols/btmidman_inet.h>

#include <QMessageBox>
#include <QCloseEvent>

static constexpr std::uint32_t MAX_PASSWORD_LENGTH = 16;

btnet_dialog::btnet_dialog(QWidget *parent, eka2l1::config::state &conf)
    : QDialog(parent)
    , ui_(new Ui::btnet_dialog)
    , conf_(conf)
    , modded_(false)
    , modded_once_(false) {
    setAttribute(Qt::WA_DeleteOnClose);
    ui_->setupUi(this);

    int correct_mode = eka2l1::common::min<int>(conf.btnet_discovery_mode,ui_->mode_combo_box->count());
    ui_->mode_combo_box->setCurrentIndex(correct_mode);
    show_opts_accord_to_index(correct_mode);

    ui_->enable_upnp_checkbox->setChecked(conf.enable_upnp);
    ui_->password_line_edit->setText(QString::fromStdString(conf.btnet_password.substr(0, MAX_PASSWORD_LENGTH)));
    ui_->server_addr_edit_line->setText(QString::fromStdString(conf.bt_central_server_url));
    ui_->password_line_edit->setInputMask("NNNNNNNNNNNNNNNN");
}

btnet_dialog::~btnet_dialog() {
    delete ui_;
}

void btnet_dialog::on_mode_combo_box_activated(int index) {
    if (index == conf_.btnet_discovery_mode) {
        return;
    }

    modded_ = true;
    show_opts_accord_to_index(index);
}

void btnet_dialog::show_opts_accord_to_index(int index) {
    ui_->server_opts_widget->hide();
    ui_->direct_ip_opts_widget->hide();
    ui_->server_and_lan_shared_opts_widget->hide();

    eka2l1::epoc::bt::discovery_mode mode = static_cast<decltype(mode)>(index);
    switch (mode) {
    case eka2l1::epoc::bt::DISCOVERY_MODE_DIRECT_IP:
        ui_->direct_ip_opts_widget->show();
        break;

    case eka2l1::epoc::bt::DISCOVERY_MODE_LOCAL_LAN:
        ui_->server_and_lan_shared_opts_widget->show();
        break;

    case eka2l1::epoc::bt::DISCOVERY_MODE_PROXY_SERVER:
        ui_->server_and_lan_shared_opts_widget->show();
        ui_->server_opts_widget->show();
        break;

    default:
        break;
    }
}

void btnet_dialog::on_direct_ip_editor_open_btn_clicked() {
    emit direct_ip_editor_requested();
}

void btnet_dialog::on_save_btn_clicked() {
    conf_.enable_upnp = ui_->enable_upnp_checkbox->isChecked();
    conf_.btnet_password = ui_->password_line_edit->text().toStdString();
    conf_.bt_central_server_url = ui_->server_addr_edit_line->text().toStdString();
    conf_.btnet_discovery_mode = static_cast<std::uint32_t>(ui_->mode_combo_box->currentIndex());

    conf_.serialize();

    modded_once_ = modded_;
    modded_ = false;
}

void btnet_dialog::on_enable_upnp_checkbox_clicked(bool toggled) {
    modded_ = true;
}

void btnet_dialog::on_password_line_edit_textEdited(const QString &text) {
    modded_ = true;
}

void btnet_dialog::on_server_addr_edit_line_textEdited(const QString &text) {
    modded_ = true;
}

void btnet_dialog::closeEvent(QCloseEvent *evt) {
    if (modded_ && QMessageBox::question(this, tr("Configuration not saved"), tr("The Bluetooth netplay configuration has not been saved. Do you want to save?")) == QMessageBox::Yes) {
        conf_.serialize();

        modded_once_ = true;
        modded_ = false;
    }

    if (modded_once_) {
        QMessageBox::information(this, tr("Relaunch needed"), tr("This change will be effective on the next launch of the emulator."));
    }

    evt->accept();
}

void btnet_dialog::on_password_clear_btn_clicked() {
    ui_->password_line_edit->clear();

    if (conf_.btnet_password != "") {
        modded_ = true;
    }
}

void btnet_dialog::on_password_gen_random_btn_clicked() {
    static const QString chars_dict("abcdefghijklmnoupqrstuvwxyz0123456789ABCDEFGHIJKLMNOUPQRSTUVWYZ");
    static const std::uint32_t chars_dict_count = static_cast<std::uint32_t>(chars_dict.length());

    QString result_string;

    for (std::uint32_t i = 0; i < MAX_PASSWORD_LENGTH; i++) {
        result_string.push_back(chars_dict.at(eka2l1::random_range(0, chars_dict_count - 1)));
    }

    ui_->password_line_edit->setText(result_string);
    modded_ = true;
}
