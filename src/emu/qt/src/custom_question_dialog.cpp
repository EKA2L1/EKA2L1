/*
 * Copyright (c) 2023 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * A portion of the code is borrowed from MainWindow.cpp in DolphinQt
 * source folder.
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

#include <qt/custom_question_dialog.h>
#include <QPushButton>

message_box_asyncable_close::message_box_asyncable_close(QWidget *parent,
    const std::u16string &text,
    const std::u16string &button1_text,
    const std::u16string &button2_text,
    eka2l1::drivers::ui::yes_no_dialog_complete_callback complete_callback)
    : QMessageBox(parent)
    , complete_callback_(complete_callback) {
    setText(QString::fromStdU16String(text));

    if (button2_text.empty()) {
        yes_button_ = addButton(QMessageBox::Yes);
    } else {
        yes_button_ = addButton(QString::fromStdU16String(button2_text), QMessageBox::YesRole);
    }

    if (button1_text.empty()) {
        no_button_ = addButton(QMessageBox::No);
    } else {
        no_button_ = addButton(QString::fromStdU16String(button1_text), QMessageBox::NoRole);
    }

    setModal(true);
    setAttribute(Qt::WA_DeleteOnClose, true);

    connect(yes_button_, &QPushButton::pressed, this, &message_box_asyncable_close::on_yes_button_clicked);
    connect(no_button_, &QPushButton::pressed, this, &message_box_asyncable_close::on_no_button_clicked);
}

void message_box_asyncable_close::on_yes_button_clicked() {
    if (complete_callback_ != nullptr) {
        complete_callback_(1);
    }
}

void message_box_asyncable_close::on_no_button_clicked() {
    if (complete_callback_ != nullptr) {
        complete_callback_(0);
    }
}