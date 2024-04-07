/*
* Copyright (c) 2024 EKA2L1 Team.
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

#include <QCheckBox>
#include <QDialog>
#include <QListWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class batch_install_dialog;
}
QT_END_NAMESPACE

class batch_install_dialog : public QDialog {
    Q_OBJECT

private:
    Ui::batch_install_dialog *ui_;

    QListWidget *list_widget_;
    QCheckBox *ignore_msgs_check_box_;
    QCheckBox *auto_lang_check_box_;

    QList<QString> files_;

    void update_install_list();

private slots:
    void on_add_files_button_clicked();
    void on_clear_files_button_clicked();
    void on_start_install_clicked();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void install_multiple(bool should_ignore_sis_msg, bool should_auto_choose_language, QList<QString> files);

public:
    explicit batch_install_dialog(QWidget *parent = nullptr, QList<QString> initial_files = {});
    ~batch_install_dialog();
};