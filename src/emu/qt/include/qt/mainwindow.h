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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <drivers/input/common.h>
#include <drivers/graphics/context.h>
#include <drivers/ui/input_dialog.h>

#include <QActionGroup>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QPointer>
#include <QProgressDialog>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui {
    class main_window;
}
QT_END_NAMESPACE

static constexpr std::size_t MAX_RECENT_ENTRIES = 5;
class symbian_input_dialog;

namespace eka2l1 {
    class system;
    class window_server;

    namespace config {
        struct app_setting;
    }

    namespace desktop {
        struct emulator;
    }

    namespace epoc {
        struct screen;
        struct window_group;
    }
}

class applist_widget;
class applist_widget_item;
class display_widget;
class settings_dialog;

class main_window : public QMainWindow {
    Q_OBJECT

private:
    QApplication &application_;

    eka2l1::desktop::emulator &emulator_state_;

    int active_screen_number_;
    std::size_t active_screen_draw_callback_;
    std::size_t active_screen_mode_change_callback_;
    std::size_t active_screen_focus_change_callback_;

    QProgressDialog *current_progress_dialog_;

    QMenu *recent_mount_folder_menu_;
    QAction *recent_mount_folder_actions_[MAX_RECENT_ENTRIES];
    QAction *recent_mount_clear_;

    QLabel *current_device_label_;
    QLabel *screen_status_label_;

    QPointer<settings_dialog> settings_dialog_;
    QActionGroup *rotate_group_;

    QMargins before_margins_;

    eka2l1::drivers::ui::input_dialog_complete_callback input_complete_callback_;
    std::u16string input_initial_text_;
    int input_text_max_len_;

    symbian_input_dialog *input_dialog_;

    void setup_screen_draw();
    void setup_app_list();
    void setup_package_installer_ui_hooks();
    void refresh_recent_mounts();
    void refresh_current_device_label();
    void refresh_mount_availbility();
    void mount_game_card_dump(QString path);
    void spawn_package_install_camper(QString package_install_path);
    void make_default_binding_profile();
    void on_screen_current_group_change_callback();
    void switch_to_game_display_mode();
    void force_refresh_applist();

    eka2l1::epoc::screen *get_current_active_screen();
    eka2l1::config::app_setting *get_active_app_setting();

    void save_ui_layouts();
    void restore_ui_layouts();

private slots:
    void on_about_triggered();
    void on_settings_triggered();
    void on_package_manager_triggered();
    void on_package_install_clicked();
    void on_device_install_clicked();
    void on_progress_dialog_change(const std::size_t now, const std::size_t total);
    bool on_package_install_text_ask(const char *text, const bool one_button);
    void on_new_device_added();
    void on_mount_card_clicked();
    void on_mount_zip_clicked();
    void on_recent_mount_clear_clicked();
    void on_recent_mount_card_folder_clicked();
    void on_status_bar_update(const std::uint64_t fps);
    void on_fullscreen_toogled(bool checked);
    void on_cursor_visibility_change(bool visible);
    void on_status_bar_visibility_change(bool visible);
    void on_restart_requested();
    void on_device_set_requested(const int index = -1);
    void on_app_setting_changed();
    void on_another_rotation_triggered(QAction *action);
    void on_pause_toggled(bool checked);
    void on_package_uninstalled();
    void on_refresh_app_list_requested();

    int on_package_install_language_choose(const int *languages, const int language_count);

    void on_app_clicked(applist_widget_item *item);
    void on_relaunch_request();

    void on_theme_change_requested(const QString &text);
    void force_update_display_minimum_size();
    void on_window_title_setting_changed();
    void on_finished_text_input(const QString &text, const bool force_close);
    void on_input_dialog_open_request();
    void on_input_dialog_close_request();
    void on_hide_system_apps_changed();

signals:
    void progress_dialog_change(const std::size_t now, const std::size_t total);
    void status_bar_update(const std::uint64_t fps);
    bool package_install_text_ask(const char *text, const bool one_button);
    int package_install_language_choose(const int *languages, const int language_count);
    void app_launching();
    void restart_requested();
    void controller_button_press(eka2l1::drivers::input_event event);
    void screen_focus_group_changed();
    void input_dialog_open_request();
    void input_dialog_close_request();

public:
    main_window(QApplication &app, QWidget *parent, eka2l1::desktop::emulator &emulator_state);
    ~main_window();

    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

    void setup_and_switch_to_game_mode();

    display_widget *render_window() {
        return displayer_;
    }

    bool controller_event_handler(eka2l1::drivers::input_event &event);

    bool input_dialog_open(const std::u16string &inital_text, const int max_length, eka2l1::drivers::ui::input_dialog_complete_callback complete_callback);
    void input_dialog_close();

private:
    Ui::main_window *ui_;
    applist_widget *applist_;
    display_widget *displayer_;
};
#endif // MAINWINDOW_H
