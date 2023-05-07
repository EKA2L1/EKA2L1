/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>
#include <QLabel>

namespace Ui {
    class about_dialog;
}

class about_dialog : public QDialog {
    Q_OBJECT

private:
    QLabel *main_developer_label_;
    QLabel *contributor_label_;
    QLabel *icon_label_;
    QLabel *honor_label_;
    QLabel *translator_label_;
    QLabel *version_label_;
    QLabel *copyright_label_;
    QLabel *special_thanks_label_;

public:
    explicit about_dialog(QWidget *parent = nullptr);
    ~about_dialog();

private:
    Ui::about_dialog *ui_;
};

#endif // ABOUTDIALOG_H
