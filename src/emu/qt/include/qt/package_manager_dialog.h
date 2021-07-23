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

#ifndef PACKAGE_MANAGER_DIALOG_H
#define PACKAGE_MANAGER_DIALOG_H

#include <QDialog>
#include <QTableWidgetItem>

namespace Ui {
    class package_manager_dialog;
}

namespace eka2l1 {
    namespace manager {
        class packages;
    }
}

class package_manager_table_info_item : public QTableWidgetItem {
private:
    std::uint32_t uid_;
    int index_;

public:
    package_manager_table_info_item(const std::uint32_t uid, const int index);

    std::uint32_t uid() const {
        return uid_;
    }

    int index() const {
        return index_;
    }
};

class package_manager_dialog : public QDialog {
    Q_OBJECT

private:
    eka2l1::manager::packages *packages_;
    package_manager_table_info_item *current_item_;

    void refresh_package_manager_items();

private slots:
    void on_search_bar_edit(const QString &new_text);
    void on_custom_menu_requested(const QPoint &pos);

    void on_package_uninstall_requested();
    void on_package_list_file_requested();

public:
    explicit package_manager_dialog(QWidget *parent, eka2l1::manager::packages *packages);
    ~package_manager_dialog();

private:
    Ui::package_manager_dialog *ui_;
};

#endif // PACKAGE_MANAGER_DIALOG_H
