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

#include <QDialog>

namespace Ui {
class SymbianInputDialog;
}

class symbian_input_dialog : public QDialog
{
    Q_OBJECT

private:
    int max_length_;

private slots:
    void event_content_changed();
    void event_submit_pressed();
    void event_finished(int result);

signals:
    void finished_input(const QString &result, const bool force_close);

public:
    explicit symbian_input_dialog(QWidget *parent = nullptr);
    ~symbian_input_dialog();

    void open_for_input(const QString &initial_text, const int max_length);
    QString get_current_text() const;

private:
    Ui::SymbianInputDialog *ui;
};
