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

#ifndef APPLISTWIDGET_H
#define APPLISTWIDGET_H

#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>

#include <mutex>

#include <common/sync.h>

namespace eka2l1 {
    class applist_server;
    class fbs_server;
    class io_system;

    struct apa_app_registry;
    class device_manager;

    namespace config {
        struct state;
    }

    namespace j2me {
        class app_list;
        struct app_entry;
    }
}

class applist_widget_item : public QListWidgetItem {
public:
    int registry_index_;
    bool is_j2me_;

    applist_widget_item(const QIcon &icon, const QString &name, int registry_index, bool is_j2me, QListWidget *listview);
};

class applist_search_bar : public QWidget {
    Q_OBJECT;

private:
    QLabel *search_label_;
    QLineEdit *search_line_edit_;
    QHBoxLayout *search_layout_;

private slots:
    void on_search_bar_content_changed(QString content);

signals:
    void new_search(QString content);

public:
    applist_search_bar(QWidget *parent = nullptr);
    ~applist_search_bar();

    const QString value() const;
};

class applist_device_combo: public QWidget {
    Q_OBJECT;

private:
    QLabel *device_label_;
    QComboBox *device_combo_;
    QHBoxLayout *device_layout_;
    int current_index_;

private slots:
    void on_device_combo_changed(int index);

signals:
    void device_combo_changed(int index);

public:
    applist_device_combo(QWidget *parent = nullptr);
    ~applist_device_combo();

    void update_devices(const QStringList &devices, const int index);
};

class applist_widget : public QWidget {
    Q_OBJECT;

public:
    applist_search_bar *search_bar_;
    applist_device_combo *device_combo_bar_;
    QListWidget *list_widget_;
    QListWidget *list_j2me_widget_;
    QVBoxLayout *layout_;
    QWidget *bar_widget_;
    QWidget *loading_widget_;
    QMovie *loading_gif_;
    QLabel *loading_label_;
    QPushButton *j2me_mode_btn_;

    QLabel *no_app_visible_normal_label_;
    QLabel *no_app_visible_hide_sysapp_label_;

    eka2l1::applist_server *lister_;
    eka2l1::fbs_server *fbss_;
    eka2l1::io_system *io_;
    eka2l1::j2me::app_list *lister_j2me_;

    bool hide_system_apps_;
    bool scanning_;
    bool loaded_[2];

    bool should_dead_;
    std::mutex exit_mutex_;
    eka2l1::common::event scanning_done_evt_;

    eka2l1::config::state &conf_;
    applist_widget_item *context_j2me_item_;

    void add_registeration_item_native(eka2l1::apa_app_registry &reg, const int index);
    void add_registeration_item_j2me(const eka2l1::j2me::app_entry &entry);

    eka2l1::apa_app_registry *get_registry_from_widget_item(applist_widget_item *item);

    void hide_all();
    void show_all();
    void on_search_content_changed(QString content);
    void update_devices(const QStringList &devices, const int index);
    void update_j2me_button_name();
    void reload_whole_list();
    void show_no_apps_avail();

private slots:
    void on_list_widget_item_clicked(QListWidgetItem *item);
    void on_device_change_request(int index);
    void on_new_registeration_item_come(QListWidgetItem *item);
    void on_j2me_mode_btn_clicked();
    void on_j2me_list_widget_custom_menu_requested(const QPoint &pos);
    void on_action_delete_j2me_app_triggered();
    void on_action_rename_j2me_app_triggered();

signals:
    void app_launch(applist_widget_item *item);
    void device_change_request(int index);
    void new_registeration_item_come(QListWidgetItem *item);

public:
    explicit applist_widget(QWidget *parent, eka2l1::applist_server *lister, eka2l1::fbs_server *fbss, eka2l1::io_system *io,
        eka2l1::j2me::app_list *lister_j2me, eka2l1::config::state &conf, const bool hide_system_apps = true, const bool ngage_mode = false);
    ~applist_widget();

    bool launch_from_widget_item(applist_widget_item *item);
    void set_hide_system_apps(const bool should_hide);
    void update_devices(eka2l1::device_manager *mngr);
    void request_reload(const bool j2me);
    void request_reload();
};

#endif // APPLISTWIDGET_H
