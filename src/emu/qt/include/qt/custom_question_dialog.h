/*
 * Copyright (c) 2023 EKA2L1 Team.
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

#include <drivers/ui/input_dialog.h>
#include <QMessageBox>

class message_box_asyncable_close: public QMessageBox {
private:
    QPushButton *yes_button_;
    QPushButton *no_button_;

    eka2l1::drivers::ui::yes_no_dialog_complete_callback complete_callback_;

protected slots:
    void on_yes_button_clicked();
    void on_no_button_clicked();

public:
    explicit message_box_asyncable_close(QWidget *parent,
        const std::u16string &text,
        const std::u16string &button1_text,
        const std::u16string &button2_text,
        eka2l1::drivers::ui::yes_no_dialog_complete_callback complete_callback);
};