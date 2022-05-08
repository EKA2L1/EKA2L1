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

#include "./ui_mainwindow.h"
#include <qt/aboutdialog.h>
#include <qt/applistwidget.h>
#include <qt/device_install_dialog.h>
#include <qt/displaywidget.h>
#include <qt/mainwindow.h>
#include <qt/symbian_input_dialog.h>
#include <qt/package_manager_dialog.h>
#include <qt/settings_dialog.h>
#include <qt/state.h>
#include <qt/utils.h>

#include <kernel/kernel.h>
#include <system/devices.h>
#include <system/epoc.h>

#include <common/algorithm.h>
#include <common/fileutils.h>
#include <common/language.h>
#include <common/path.h>
#include <common/platform.h>
#include <common/rgb.h>

#include <config/app_settings.h>

#include <package/manager.h>
#include <services/applist/applist.h>
#include <services/fbs/fbs.h>
#include <services/ui/cap/oom_app.h>
#include <services/window/classes/wingroup.h>
#include <services/window/screen.h>
#include <services/window/window.h>

#include <QCheckBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFuture>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QtConcurrent/QtConcurrent>

static constexpr const char *LAST_UI_WINDOW_GEOMETRY_SETTING = "lastWindowGeometry";
static constexpr const char *LAST_UI_WINDOW_STATE = "lastWindowState";
static constexpr const char *LAST_EMULATED_DISPLAY_GEOMETRY_SETTING = "lastEmulatedDisplayGeometry";
static constexpr const char *LAST_EMULATED_DISPLAY_STATE = "lastEmulatedDisplayState";
static constexpr const char *LAST_PACKAGE_FOLDER_SETTING = "lastPackageFolder";
static constexpr const char *LAST_MOUNT_FOLDER_SETTING = "lastMountFolder";
static constexpr const char *NO_DEVICE_INSTALL_DISABLE_NOF_SETTING = "disableNoDeviceInstallNotify";
static constexpr const char *NO_TOUCHSCREEN_DISABLE_WARN_SETTING = "disableNoTouchscreenWarn";

static void mode_change_screen(void *userdata, eka2l1::epoc::screen *scr, const int old_mode) {
    eka2l1::desktop::emulator *state_ptr = reinterpret_cast<eka2l1::desktop::emulator *>(userdata);
    if (!state_ptr) {
        return;
    }

    QSize new_minsize(scr->current_mode().size.x, scr->current_mode().size.y);
    if ((scr->ui_rotation % 180) != 0) {
        new_minsize = QSize(scr->current_mode().size.y, scr->current_mode().size.x);
    }

    QSettings settings;
    QVariant allow_true_size_variant = settings.value(TRUE_SIZE_RESIZE_SETTING_NAME, false);

    display_widget *widget = static_cast<display_widget *>(state_ptr->window);
    if (allow_true_size_variant.toBool()) {
        new_minsize /= widget->devicePixelRatioF();
    }

    widget->setMinimumSize(new_minsize);
}

static void draw_emulator_screen(void *userdata, eka2l1::epoc::screen *scr, const bool is_dsa, const bool need_wait = true) {
    eka2l1::desktop::emulator *state_ptr = reinterpret_cast<eka2l1::desktop::emulator *>(userdata);
    if (!state_ptr || !state_ptr->graphics_driver) {
        return;
    }
    
    if (need_wait)
        state_ptr->graphics_driver->wait_for(&state_ptr->present_status);

    eka2l1::desktop::emulator &state = *state_ptr;
    eka2l1::drivers::graphics_command_builder builder;

    eka2l1::rect viewport;
    eka2l1::rect src;
    eka2l1::rect dest;

    eka2l1::drivers::filter_option filter = state.conf.nearest_neighbor_filtering ? eka2l1::drivers::filter_option::nearest : eka2l1::drivers::filter_option::linear;

    const auto window_width = state.window->window_fb_size().x;
    const auto window_height = state.window->window_fb_size().y;

    eka2l1::vec2 swapchain_size(window_width, window_height);
    viewport.size = swapchain_size;
    builder.set_swapchain_size(swapchain_size);
    builder.backup_state();

    eka2l1::vec4 color_clear = eka2l1::common::rgba_to_vec(state.conf.display_background_color.load());

    // The format that is stored is same as how it's present in HTML ARGB (from lowest to highest bytes)
    // The normal one that emulator assumes is ABGR (from lowest to highest bytes too)
    builder.set_feature(eka2l1::drivers::graphics_feature::cull, false);
    builder.set_feature(eka2l1::drivers::graphics_feature::depth_test, false);
    builder.set_feature(eka2l1::drivers::graphics_feature::blend, false);
    builder.set_feature(eka2l1::drivers::graphics_feature::stencil_test, false);
    builder.set_feature(eka2l1::drivers::graphics_feature::clipping, false);
    builder.set_viewport(viewport);

    builder.clear({ color_clear.z / 255.0f, color_clear.y / 255.0f, color_clear.x / 255.0f, color_clear.w / 255.0f, 0.0f, 0.0f }, eka2l1::drivers::draw_buffer_bit_color_buffer);

    auto &crr_mode = scr->current_mode();

    eka2l1::vec2 size = crr_mode.size;
    if ((scr->ui_rotation % 180) != 0) {
        std::swap(size.x, size.y);
    }

    float mult = scr->requested_ui_scale_factor > 0.0f ? scr->requested_ui_scale_factor : (static_cast<float>(window_width) / size.x);
    float width = size.x * mult;
    float height = size.y * mult;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    if (height > swapchain_size.y) {
        height = swapchain_size.y;
        mult = height / size.y;
        width = size.x * mult;
    }

    x = (swapchain_size.x - width) / 2;
    y = (swapchain_size.y - height) / 2;

    scr->set_native_scale_factor(state_ptr->graphics_driver.get(), mult, mult);
    scr->absolute_pos.x = static_cast<int>(x);
    scr->absolute_pos.y = static_cast<int>(y);

    dest.top = eka2l1::vec2(x, y);
    dest.size = eka2l1::vec2(width, height);

    eka2l1::drivers::advance_draw_pos_around_origin(dest, scr->ui_rotation);

    if (scr->ui_rotation % 180 != 0) {
        std::swap(dest.size.x, dest.size.y);
    }

    builder.set_texture_filter(scr->screen_texture, true, filter);
    builder.set_texture_filter(scr->screen_texture, false, filter);

    builder.draw_bitmap(scr->screen_texture, 0, dest, src, eka2l1::vec2(0, 0), static_cast<float>(scr->ui_rotation),
        (scr->flags_ & eka2l1::epoc::screen::FLAG_SCREEN_UPSCALE_FACTOR_LOCK) ? eka2l1::drivers::bitmap_draw_flag_use_upscale_shader : 0);

    builder.load_backup_state();

    state_ptr->present_status = -100;

    // Submit, present, and wait for the presenting
    builder.present(&state_ptr->present_status);

    eka2l1::drivers::command_list retrieved = builder.retrieve_command_list();
    state.graphics_driver->submit_command_list(retrieved);
}


main_window::main_window(QApplication &application, QWidget *parent, eka2l1::desktop::emulator &emulator_state)
    : QMainWindow(parent)
    , application_(application)
    , emulator_state_(emulator_state)
    , active_screen_number_(0)
    , active_screen_draw_callback_(0)
    , active_screen_mode_change_callback_(0)
    , settings_dialog_(nullptr)
    , input_complete_callback_(nullptr)
    , input_text_max_len_(0x7FFFFFFF)
    , input_dialog_(nullptr)
    , ui_(new Ui::main_window)
    , applist_(nullptr)
    , displayer_(nullptr) {
    ui_->setupUi(this);
    ui_->label_al_not_available->setVisible(false);

    eka2l1::kernel_system *kernel = emulator_state_.symsys->get_kernel_system();

    if (kernel && kernel->get_thread_list().empty())
        setup_app_list();

    setup_package_installer_ui_hooks();

    if (!applist_) {
        ui_->label_al_not_available->setVisible(true);
    }

    displayer_ = new display_widget(this);
    displayer_->setVisible(false);

    ui_->layout_main->addWidget(displayer_);

    recent_mount_folder_menu_ = new QMenu(this);

    recent_mount_clear_ = new QAction(this);
    recent_mount_clear_->setText(tr("Clear menu"));

    connect(recent_mount_clear_, &QAction::triggered, this, &main_window::on_recent_mount_clear_clicked);

    for (std::size_t i = 0; i < MAX_RECENT_ENTRIES; i++) {
        recent_mount_folder_actions_[i] = new QAction(this);
        recent_mount_folder_actions_[i]->setVisible(false);

        recent_mount_folder_menu_->addAction(recent_mount_folder_actions_[i]);
        connect(recent_mount_folder_actions_[i], &QAction::triggered, this, &main_window::on_recent_mount_card_folder_clicked);
    }

    recent_mount_folder_menu_->addSeparator();
    recent_mount_folder_menu_->addAction(recent_mount_clear_);

    refresh_recent_mounts();

    current_device_label_ = new QLabel(this);
    screen_status_label_ = new QLabel(this);

    current_device_label_->setAlignment(Qt::AlignCenter);
    screen_status_label_->setAlignment(Qt::AlignRight);

    ui_->status_bar->addPermanentWidget(current_device_label_, 1);
    ui_->status_bar->addPermanentWidget(screen_status_label_, 1);

    ui_->action_pause->setEnabled(false);
    ui_->action_restart->setEnabled(false);

    addAction(ui_->action_fullscreen);

    refresh_current_device_label();
    make_default_binding_profile();
    refresh_mount_availbility();

    eka2l1::window_server *server = get_window_server_through_system(emulator_state_.symsys.get());
    if (server) {
        server->init_key_mappings();
    }

    rotate_group_ = new QActionGroup(this);
    rotate_group_->addAction(ui_->action_rotate_0);
    rotate_group_->addAction(ui_->action_rotate_90);
    rotate_group_->addAction(ui_->action_rotate_180);
    rotate_group_->addAction(ui_->action_rotate_270);

    ui_->action_rotate_drop_menu->setDisabled(true);
    ui_->action_fullscreen->setChecked(emulator_state_.init_fullscreen);

    QSettings settings;

    ui_->status_bar->setVisible(!settings.value(STATUS_BAR_HIDDEN_SETTING_NAME, false).toBool());
    active_screen_number_ = settings.value(SHOW_SCREEN_NUMBER_SETTINGS_NAME, 0).toInt();

    restore_ui_layouts();
    on_theme_change_requested(QString("%1").arg(settings.value(THEME_SETTING_NAME, 0).toInt()));

    QVariant no_notify_install = settings.value(NO_TOUCHSCREEN_DISABLE_WARN_SETTING);

    if (!no_notify_install.isValid() || !no_notify_install.toBool()) {
        for (auto &bind : emulator_state_.conf.keybinds.keybinds) {
            if (bind.source.type == eka2l1::config::KEYBIND_TYPE_MOUSE) {
                make_dialog_with_checkbox_and_choices(
                    tr("Touchscreen disabled"), tr("Some of your current keybinds are associated with mouse buttons. Therefore emulated touchscreen is disabled.<br><br><b>Note:</b><br>Touchscreen can be re-enabled by rebinding mouse buttons with keyboard keys."),
                    tr("Don't show this again"), false, [](bool on) {
                        QSettings settings;
                        settings.setValue(NO_TOUCHSCREEN_DISABLE_WARN_SETTING, on);
                    },
                    false);

                break;
            }
        }
    }
    
    QColor default_color = settings.value(BACKGROUND_COLOR_DISPLAY_SETTING_NAME, QColor(0xD0, 0xD0, 0xD0)).value<QColor>();
    emulator_state_.conf.display_background_color = default_color.rgba();

    connect(ui_->action_about, &QAction::triggered, this, &main_window::on_about_triggered);
    connect(ui_->action_settings, &QAction::triggered, this, &main_window::on_settings_triggered);
    connect(ui_->action_package, &QAction::triggered, this, &main_window::on_package_install_clicked);
    connect(ui_->action_device, &QAction::triggered, this, &main_window::on_device_install_clicked);
    connect(ui_->action_mount_game_card_folder, &QAction::triggered, this, &main_window::on_mount_card_clicked);
    connect(ui_->action_mount_game_card_zip, &QAction::triggered, this, &main_window::on_mount_zip_clicked);
    connect(ui_->action_fullscreen, &QAction::toggled, this, &main_window::on_fullscreen_toogled);
    connect(ui_->action_pause, &QAction::toggled, this, &main_window::on_pause_toggled);
    connect(ui_->action_restart, &QAction::triggered, this, &main_window::on_restart_requested);
    connect(ui_->action_package_manager, &QAction::triggered, this, &main_window::on_package_manager_triggered);
    connect(ui_->action_refresh_app_list, &QAction::triggered, this, &main_window::on_refresh_app_list_requested);

    connect(rotate_group_, &QActionGroup::triggered, this, &main_window::on_another_rotation_triggered);

    connect(this, &main_window::progress_dialog_change, this, &main_window::on_progress_dialog_change, Qt::QueuedConnection);
    connect(this, &main_window::status_bar_update, this, &main_window::on_status_bar_update, Qt::QueuedConnection);
    connect(this, &main_window::package_install_text_ask, this, &main_window::on_package_install_text_ask, Qt::BlockingQueuedConnection);
    connect(this, &main_window::package_install_language_choose, this, &main_window::on_package_install_language_choose, Qt::BlockingQueuedConnection);
    connect(this, &main_window::screen_focus_group_changed, this, &main_window::on_screen_current_group_change_callback, Qt::QueuedConnection);
    connect(this, &main_window::input_dialog_open_request, this, &main_window::on_input_dialog_open_request);
    connect(this, &main_window::input_dialog_close_request, this, &main_window::on_input_dialog_close_request);

    setAcceptDrops(true);
}

void main_window::setup_app_list() {
    if (applist_)
        return;

    eka2l1::system *system = emulator_state_.symsys.get();
    eka2l1::kernel_system *kernel = system->get_kernel_system();
    if (kernel) {
        const std::string al_server_name = eka2l1::get_app_list_server_name_by_epocver(kernel->get_epoc_version());
        const std::string fbs_server_name = eka2l1::epoc::get_fbs_server_name_by_epocver(kernel->get_epoc_version());

        eka2l1::applist_server *al_serv = reinterpret_cast<eka2l1::applist_server *>(kernel->get_by_name<eka2l1::service::server>(al_server_name));
        eka2l1::fbs_server *fbs_serv = reinterpret_cast<eka2l1::fbs_server *>(kernel->get_by_name<eka2l1::service::server>(fbs_server_name));

        if (al_serv && fbs_serv) {
            applist_ = new applist_widget(this, al_serv, fbs_serv, system->get_io_system(), true);
            ui_->layout_main->addWidget(applist_);

            connect(applist_, &applist_widget::app_launch, this, &main_window::on_app_clicked);
        }
    }

    if (!applist_) {
        QSettings settings;
        QVariant no_notify_install = settings.value(NO_DEVICE_INSTALL_DISABLE_NOF_SETTING);

        if (!no_notify_install.isValid() || !no_notify_install.toBool()) {
            const QMessageBox::StandardButton result = make_dialog_with_checkbox_and_choices(
                tr("No device installed"), tr("You have not installed any device. Please install a device or follow the installation instructions on EKA2L1's GitHub wiki page."),
                tr("Don't show this again"), false, [](bool on) {
                    QSettings settings;
                    settings.setValue(NO_DEVICE_INSTALL_DISABLE_NOF_SETTING, on);
                },
                true);

            if (result == QMessageBox::Ok) {
                on_device_install_clicked();
            }
        }
    }
}

void main_window::setup_package_installer_ui_hooks() {
    eka2l1::system *system = emulator_state_.symsys.get();
    eka2l1::manager::packages *pkgmngr = system->get_packages();

    pkgmngr->show_text = [this](const char *text, const bool one_button) -> bool {
        // We only have one receiver, so it's ok ;)
        return emit package_install_text_ask(text, one_button);
    };

    pkgmngr->choose_lang = [this](const int *languages, const int language_count) -> int {
        return emit package_install_language_choose(languages, language_count);
    };
}

eka2l1::epoc::screen *main_window::get_current_active_screen() {
    return ::get_current_active_screen(emulator_state_.symsys.get(), active_screen_number_);
}

eka2l1::config::app_setting *main_window::get_active_app_setting() {
    return ::get_active_app_setting(emulator_state_.symsys.get(), *emulator_state_.app_settings, active_screen_number_);
}

main_window::~main_window() {
    if (applist_) {
        delete applist_;
    }

    if (displayer_) {
        delete displayer_;
    }

    if (input_dialog_) {
        delete input_dialog_;
    }

    delete recent_mount_folder_menu_;
    delete recent_mount_clear_;

    for (std::size_t i = 0; i < MAX_RECENT_ENTRIES; i++) {
        delete recent_mount_folder_actions_[i];
    }

    delete current_device_label_;
    delete screen_status_label_;

    delete rotate_group_;
    delete ui_;
}

void main_window::refresh_current_device_label() {
    eka2l1::device_manager *dvcmngr = emulator_state_.symsys->get_device_manager();
    if (dvcmngr) {
        eka2l1::device *crr = dvcmngr->get_current();
        if (crr) {
            current_device_label_->setText(QString("%1 (%2)").arg(QString::fromStdString(crr->model.c_str()), QString::fromStdString(crr->firmware_code)));
        }
    }
}

void main_window::on_about_triggered() {
    about_dialog *about_diag = new about_dialog(this);
    about_diag->show();
}

void main_window::on_settings_triggered() {
    if (!settings_dialog_) {
        settings_dialog_ = new settings_dialog(this, emulator_state_.symsys.get(), emulator_state_.joystick_controller.get(),
            emulator_state_.app_settings.get(), emulator_state_.conf);

        connect(settings_dialog_.data(), &settings_dialog::cursor_visibility_change, this, &main_window::on_cursor_visibility_change);
        connect(settings_dialog_.data(), &settings_dialog::status_bar_visibility_change, this, &main_window::on_status_bar_visibility_change);
        connect(settings_dialog_.data(), &settings_dialog::relaunch, this, &main_window::on_relaunch_request);
        connect(settings_dialog_.data(), &settings_dialog::restart, this, &main_window::on_device_set_requested);
        connect(settings_dialog_.data(), &settings_dialog::theme_change_request, this, &main_window::on_theme_change_requested);
        connect(settings_dialog_.data(), &settings_dialog::minimum_display_size_change, this, &main_window::force_update_display_minimum_size);
        connect(settings_dialog_.data(), &settings_dialog::active_app_setting_changed, this, &main_window::on_app_setting_changed, Qt::DirectConnection);
        connect(settings_dialog_.data(), &settings_dialog::window_title_setting_changed, this, &main_window::on_window_title_setting_changed);
        connect(this, &main_window::app_launching, settings_dialog_.data(), &settings_dialog::on_app_launching);
        connect(this, &main_window::controller_button_press, settings_dialog_.data(), &settings_dialog::on_controller_button_press);
        connect(this, &main_window::screen_focus_group_changed, settings_dialog_.data(), &settings_dialog::refresh_app_configuration_details);
        connect(this, &main_window::restart_requested, settings_dialog_.data(), &settings_dialog::on_restart_requested_from_main);

        settings_dialog_->show();
    } else {
        settings_dialog_->raise();
    }
}

void main_window::force_refresh_applist() {
    // Try to refersh app lists
    if (applist_ && applist_->lister_->rescan_registries(applist_->io_)) {
        applist_->reload_whole_list();
    }
}

void main_window::on_package_manager_triggered() {
    if (emulator_state_.symsys) {
        eka2l1::manager::packages *pkgmngr = emulator_state_.symsys->get_packages();
        if (pkgmngr) {
            package_manager_dialog *mgdiag = new package_manager_dialog(this, pkgmngr);
            connect(mgdiag, &package_manager_dialog::package_uninstalled, this, &main_window::on_package_uninstalled);

            mgdiag->exec();
        }
    }
}

void main_window::on_package_uninstalled() {
    force_refresh_applist();
}

void main_window::on_device_set_requested(const int index) {
    ui_->action_rotate_drop_menu->setEnabled(false);

    save_ui_layouts();
    displayer_->setVisible(false);

    eka2l1::system *system = emulator_state_.symsys.get();
    if (system) {
        eka2l1::epoc::screen *scr = get_current_active_screen();
        if (scr) {
            scr->remove_screen_mode_change_callback(active_screen_mode_change_callback_);
            scr->remove_screen_redraw_callback(active_screen_draw_callback_);
            scr->remove_focus_change_callback(active_screen_focus_change_callback_);

            active_screen_draw_callback_ = 0;
            active_screen_mode_change_callback_ = 0;
            active_screen_focus_change_callback_ = 0;
        }
    }

    if (applist_) {
        delete applist_;
        applist_ = nullptr;
    }

    if (system) {
        if (index < 0) {
            system->reset();
        } else {
            system->set_device(static_cast<std::uint8_t>(index));
        }
    }

    restore_ui_layouts();
    setup_app_list();
    refresh_current_device_label();
    refresh_mount_availbility();
    screen_status_label_->clear();

    ui_->action_pause->setEnabled(false);
    ui_->action_restart->setEnabled(false);
    ui_->action_pause->setChecked(false);
}

void main_window::on_restart_requested() {
    on_device_set_requested(-1);
    emit restart_requested();
}

void main_window::on_pause_toggled(bool checked) {
    if (checked) {
        emulator_state_.should_emu_pause = true;
    } else {
        emulator_state_.should_emu_pause = false;
        emulator_state_.pause_event.set();
    }
}

void main_window::on_relaunch_request() {
    QString program = QApplication::applicationFilePath();
    QStringList arguments = QApplication::arguments();
    QString workingDirectory = QDir::currentPath();

    QProcess::startDetached(program, arguments.mid(1), workingDirectory);

    close();
}

void main_window::on_cursor_visibility_change(bool visible) {
    if (visible) {
        displayer_->setCursor(Qt::ArrowCursor);
    } else {
        displayer_->setCursor(Qt::BlankCursor);
    }
}

void main_window::on_status_bar_visibility_change(bool visible) {
    ui_->status_bar->setVisible(visible);
}

void main_window::on_status_bar_update(const std::uint64_t fps) {
    screen_status_label_->setText(QString("%1 FPS").arg(fps));
}

void main_window::on_new_device_added() {
    if (!applist_) {
        ui_->label_al_not_available->setVisible(false);

        emulator_state_.symsys->startup();
        emulator_state_.symsys->set_device(0);

        // Set and wait for reinitialization
        emulator_state_.init_event.set();
        emulator_state_.init_event.wait();

        refresh_current_device_label();
        setup_app_list();
    }
}

void main_window::on_device_install_clicked() {
    device_install_dialog *install_diag = new device_install_dialog(this, emulator_state_.symsys->get_device_manager(), emulator_state_.conf);
    connect(install_diag, &device_install_dialog::new_device_added, this, &main_window::on_new_device_added);

    install_diag->show();
}

void main_window::on_fullscreen_toogled(bool checked) {
    if (!displayer_->isVisible()) {
        return;
    }

    if (checked) {
        save_ui_layouts();

        ui_->status_bar->hide();
        ui_->menu_bar->setVisible(false);

        before_margins_ = ui_->layout_centralwidget->contentsMargins();
        ui_->layout_centralwidget->setContentsMargins(0, 0, 0, 0);

        showFullScreen();
    } else {
        ui_->status_bar->show();
        ui_->menu_bar->setVisible(true);

        ui_->layout_centralwidget->setContentsMargins(before_margins_);

        showNormal();
    }

    displayer_->set_fullscreen(checked);
}

void main_window::refresh_recent_mounts() {
    QSettings settings;
    QStringList recent_mount_folders = settings.value(RECENT_MOUNT_SETTINGS_NAME).toStringList();

    std::size_t i = 0;

    for (; i < eka2l1::common::min<qsizetype>(static_cast<qsizetype>(recent_mount_folders.size()), static_cast<qsizetype>(MAX_RECENT_ENTRIES)); i++) {
        recent_mount_folder_actions_[i]->setText(QString("&%1 %2").arg(i + 1).arg(recent_mount_folders[i]));
        recent_mount_folder_actions_[i]->setData(recent_mount_folders[i]);
        recent_mount_folder_actions_[i]->setVisible(true);
    }

    for (; i < MAX_RECENT_ENTRIES; i++) {
        recent_mount_folder_actions_[i]->setVisible(false);
    }

    if (recent_mount_folders.isEmpty()) {
        ui_->action_mount_recent_dumps->setVisible(false);
        ui_->action_mount_recent_dumps->setMenu(nullptr);
    } else {
        ui_->action_mount_recent_dumps->setVisible(true);
        ui_->action_mount_recent_dumps->setMenu(recent_mount_folder_menu_);
    }
}

void main_window::mount_game_card_dump(QString mount_path) {
    if (eka2l1::is_separator(static_cast<char16_t>(mount_path.back().unicode()))) {
        // We don't care much about the last separator. This is for system folder check ;)
        mount_path.remove(mount_path.length() - 2, 2);
    }

    eka2l1::io_system *io = emulator_state_.symsys->get_io_system();

    const std::string path_ext = eka2l1::path_extension(mount_path.toStdString());
    if (!eka2l1::common::is_dir(mount_path.toStdString()) && (eka2l1::common::compare_ignore_case(path_ext.c_str(), ".zip") == 0)) {
        io->unmount(drive_e);
        current_progress_dialog_ = new QProgressDialog(QString{}, tr("Cancel"), 0, 100, this);

        current_progress_dialog_->setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::WindowMaximizeButtonHint & ~Qt::WindowCloseButtonHint);
        current_progress_dialog_->setWindowTitle(tr("Extracting game dump files"));
        current_progress_dialog_->setWindowModality(Qt::ApplicationModal);
        current_progress_dialog_->show();

        QFuture<eka2l1::zip_mount_error> extract_future = QtConcurrent::run([this, mount_path]() -> eka2l1::zip_mount_error {
            return emulator_state_.symsys->mount_game_zip(
                drive_e, drive_media::physical, mount_path.toStdString(), 0, [this](const std::size_t done, const std::size_t total) { emit progress_dialog_change(done, total); }, [this] { return current_progress_dialog_->wasCanceled(); });
        });

        while (!extract_future.isFinished()) {
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        bool no_more_info = false;

        if (current_progress_dialog_->wasCanceled()) {
            no_more_info = true;
        }

        current_progress_dialog_->close();
        const QString title_dialog = tr("Mounting aborted");

        if (!no_more_info) {
            switch (extract_future.result()) {
            case eka2l1::zip_mount_error_corrupt: {
                QMessageBox::critical(this, title_dialog, tr("The ZIP file is corrupted!"));
                break;
            }

            case eka2l1::zip_mount_error_no_system_folder: {
                QMessageBox::critical(this, title_dialog, tr("The ZIP does not have System folder in the root folder. "
                                                             "System folder must exist in a game dump."));
                break;
            }

            case eka2l1::zip_mount_error_not_zip: {
                QMessageBox::critical(this, title_dialog, tr("The choosen file is not a ZIP file!"));
            }

            default:
                break;
            }
        }
    } else {
        io->unmount(drive_e);
        io->mount_physical_path(drive_e, drive_media::physical, io_attrib_removeable | io_attrib_write_protected, mount_path.toStdU16String());

        if (mount_path.endsWith("system", Qt::CaseInsensitive)) {
#if !EKA2L1_PLATFORM(WIN32)
            if (mount_path.endsWith("system", Qt::CaseSensitive)) {
                QMessageBox::information(this, tr("Game card problem"), tr("The game card dump has case-sensitive files. This may cause problems with the emulator."));
            }
#endif

            QMessageBox::StandardButton result = QMessageBox::question(this, tr("Game card dump folder correction"), tr("The selected path seems to be incorrect.<br>"
                                                                                                                        "Do you want the emulator to correct it?"));

            if (result == QMessageBox::Yes) {
                mount_path.remove(mount_path.length() - 8, 7);
            }
        }
    }

    QSettings settings;
    QStringList recent_mount_folders = settings.value(RECENT_MOUNT_SETTINGS_NAME).toStringList();

    if (recent_mount_folders.size() == MAX_RECENT_ENTRIES) {
        recent_mount_folders.removeLast();
    }

    if (recent_mount_folders.indexOf(mount_path) == -1)
        recent_mount_folders.prepend(mount_path);

    settings.setValue(RECENT_MOUNT_SETTINGS_NAME, recent_mount_folders);
    settings.sync();

    refresh_recent_mounts();

    if (applist_) {
        applist_->reload_whole_list();
    }
}

void main_window::on_recent_mount_clear_clicked() {
    QSettings settings;
    QStringList empty_list;

    // I'm utterly failure, can't write more. Pure lazy
    settings.setValue(RECENT_MOUNT_SETTINGS_NAME, empty_list);
    settings.sync();

    refresh_recent_mounts();
}

void main_window::on_recent_mount_card_folder_clicked() {
    QAction *being = qobject_cast<QAction *>(sender());
    if (being) {
        mount_game_card_dump(being->data().toString());
    }
}

void main_window::on_mount_card_clicked() {
    QSettings settings;
    QVariant last_mount_parent_variant = settings.value(LAST_MOUNT_FOLDER_SETTING);
    QString last_mount_folder;

    if (last_mount_parent_variant.isValid()) {
        last_mount_folder = last_mount_parent_variant.toString();
    }

    QString mount_folder = QFileDialog::getExistingDirectory(this, tr("Choose the game card dump folder"), last_mount_folder);
    if (!mount_folder.isEmpty()) {
        mount_game_card_dump(mount_folder);

        QDir mount_folder_dir(mount_folder);
        mount_folder_dir.cdUp();

        settings.setValue(LAST_MOUNT_FOLDER_SETTING, mount_folder_dir.absolutePath());
    }
}

void main_window::on_mount_zip_clicked() {
    QSettings settings;
    QVariant last_mount_parent_variant = settings.value(LAST_MOUNT_FOLDER_SETTING);
    QString last_mount_folder;

    if (last_mount_parent_variant.isValid()) {
        last_mount_folder = last_mount_parent_variant.toString();
    }

    QString mount_zip = QFileDialog::getOpenFileName(this, tr("Choose the game card zip"), last_mount_folder, "ZIP file (*.zip)");
    if (!mount_zip.isEmpty()) {
        mount_game_card_dump(mount_zip);

        QDir mount_folder_dir(mount_zip);
        mount_folder_dir.cdUp();

        settings.setValue(LAST_MOUNT_FOLDER_SETTING, mount_folder_dir.absolutePath());
    }
}

void main_window::setup_screen_draw() {
    eka2l1::system *system = emulator_state_.symsys.get();

    if (system) {
        eka2l1::epoc::screen *scr = get_current_active_screen();
        if (scr) {
            active_screen_draw_callback_ = scr->add_screen_redraw_callback(&emulator_state_, [this](void *userdata, eka2l1::epoc::screen *scr, const bool is_dsa) {
                draw_emulator_screen(userdata, scr, is_dsa, true);
                emit status_bar_update(scr->last_fps);
            });

            active_screen_mode_change_callback_ = scr->add_screen_mode_change_callback(&emulator_state_, mode_change_screen);
            active_screen_focus_change_callback_ = scr->add_focus_change_callback(&emulator_state_, [this](void *userdata, eka2l1::epoc::window_group *, eka2l1::epoc::focus_change_property) {
                emit screen_focus_group_changed();
            });

            mode_change_screen(&emulator_state_, scr, 0);
        }
    }
}

void main_window::switch_to_game_display_mode() {
    if (applist_)
        applist_->setVisible(false);
    else
        ui_->label_al_not_available->setVisible(false);

    save_ui_layouts();

    displayer_->setVisible(true);
    displayer_->setFocus();

    restore_ui_layouts();

    ui_->action_pause->setEnabled(true);
    ui_->action_restart->setEnabled(true);
    ui_->action_rotate_drop_menu->setEnabled(true);

    before_margins_ = ui_->layout_centralwidget->contentsMargins();
    on_fullscreen_toogled(ui_->action_fullscreen->isChecked());
}

void main_window::on_app_clicked(applist_widget_item *item) {
    if (!active_screen_draw_callback_) {
        setup_screen_draw();
    }

    if (applist_->launch_from_widget_item(item)) {
        switch_to_game_display_mode();
        emit app_launching();
    } else {
        LOG_ERROR(eka2l1::FRONTEND_UI, "Fail to launch the selected application!");
    }
}

void main_window::setup_and_switch_to_game_mode() {
    if (!active_screen_draw_callback_) {
        setup_screen_draw();
    }

    switch_to_game_display_mode();
}

void main_window::on_progress_dialog_change(const std::size_t now, const std::size_t total) {
    current_progress_dialog_->setValue(static_cast<int>(now * 100 / total));
}

bool main_window::on_package_install_text_ask(const char *text, const bool one_button) {
    const QMessageBox::StandardButton result = QMessageBox::question(this, tr("Document"), QString::fromUtf8(text),
        one_button ? QMessageBox::StandardButton::Ok : (QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No));

    if (one_button) {
        return true;
    }

    return (result == QMessageBox::StandardButton::Yes);
}

int main_window::on_package_install_language_choose(const int *languages, const int language_count) {
    QStringList language_string_list;
    for (int i = 0; i < language_count; i++) {
        const std::string lang_name_in_std = eka2l1::common::get_language_name_by_code(languages[i]);
        language_string_list.push_back(QString::fromUtf8(lang_name_in_std.c_str()));
    }

    bool go_fine = false;
    QString result = QInputDialog::getItem(this, tr("Choose a language for the package"), QString(), language_string_list, 0, false, &go_fine);
    if (result.isEmpty() || !go_fine) {
        return -1;
    }

    const int index = language_string_list.indexOf(result);
    if (index >= language_count) {
        LOG_ERROR(eka2l1::FRONTEND_UI, "The index of the choosen language is out of provided range! (index={})", index);
        return -1;
    }

    return languages[index];
}

void main_window::spawn_package_install_camper(QString package_file_path) {
    if (!package_file_path.isEmpty()) {
        eka2l1::manager::packages *pkgmngr = emulator_state_.symsys->get_packages();
        if (pkgmngr) {
            current_progress_dialog_ = new QProgressDialog(QString{}, tr("Cancel"), 0, 100, this);

            current_progress_dialog_->setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::WindowMaximizeButtonHint & ~Qt::WindowCloseButtonHint);
            current_progress_dialog_->setWindowTitle(tr("Installing package progress"));
            current_progress_dialog_->show();

            QFuture<eka2l1::package::installation_result> install_future = QtConcurrent::run([this, pkgmngr, package_file_path]() {
                return pkgmngr->install_package(
                    package_file_path.toStdU16String(), drive_e, [this](const std::size_t done, const std::size_t total) { emit progress_dialog_change(done, total); }, [this] { return current_progress_dialog_->wasCanceled(); });
            });

            while (!install_future.isFinished()) {
                QCoreApplication::processEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            bool no_more_info = false;

            if (current_progress_dialog_->wasCanceled()) {
                no_more_info = true;
            }

            current_progress_dialog_->close();

            if (!no_more_info) {
                switch (install_future.result()) {
                case eka2l1::package::installation_result_aborted: {
                    QMessageBox::information(this, tr("Installation aborted"), tr("The installation has been canceled"));
                    break;
                }

                case eka2l1::package::installation_result_invalid: {
                    QMessageBox::critical(this, tr("Installation failed"), tr("Fail to install package at path: %1. "
                                                                              "Ensure the path points to a valid SIS/SISX file.")
                                                                               .arg(package_file_path));
                    break;
                }

                case eka2l1::package::installation_result_success: {
                    QMessageBox::information(this, tr("Installation success"), tr("Package has been successfully installed"));
                    break;
                }

                default:
                    break;
                }
            }

            if (install_future.result() == eka2l1::package::installation_result_success) {
                force_refresh_applist();
            }
        }
    }
}

void main_window::on_package_install_clicked() {
    QSettings settings;
    QString last_install_dir;

    QVariant last_install_dir_variant = settings.value(LAST_PACKAGE_FOLDER_SETTING);
    if (last_install_dir_variant.isValid()) {
        last_install_dir = last_install_dir_variant.toString();
    }

    QString package_file_path = QFileDialog::getOpenFileName(this, tr("Choose the file to install"), last_install_dir, tr("SIS file (*.sis *.sisx)"));
    if (!package_file_path.isEmpty()) {
        settings.setValue(LAST_PACKAGE_FOLDER_SETTING, QFileInfo(package_file_path).absoluteDir().path());
    }
    spawn_package_install_camper(package_file_path);
}

void main_window::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);

    // Only care to redraw if displayer is active
    if (!displayer_->isVisible()) {
        return;
    }

    save_ui_layouts();

    eka2l1::system *system = emulator_state_.symsys.get();
    if (system) {
        eka2l1::epoc::screen *scr = get_current_active_screen();
        if (scr) {
            scr->flags_ |= eka2l1::epoc::screen::FLAG_SERVER_REDRAW_PENDING;
            draw_emulator_screen(&emulator_state_, scr, true, false);
        }
    }
}

void main_window::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> list_url = event->mimeData()->urls();
        if (list_url.size() == 1) {
            QString local_path = list_url[0].toLocalFile();
            if (local_path.endsWith(".sis") || local_path.endsWith(".sisx")) {
                event->acceptProposedAction();
            }
        }
    }
}

void main_window::dropEvent(QDropEvent *event) {
    QList<QUrl> list_url = event->mimeData()->urls();
    if (list_url.size() == 1) {
        QString local_path = list_url[0].toLocalFile();
        if (local_path.endsWith(".sis") || local_path.endsWith(".sisx")) {
            spawn_package_install_camper(local_path);
        }
    }
}

void main_window::closeEvent(QCloseEvent *event) {
    save_ui_layouts();
}

bool main_window::controller_event_handler(eka2l1::drivers::input_event &event) {
    emit controller_button_press(event);
    return (displayer_ && displayer_->isVisible() && displayer_->hasFocus());
}

void main_window::make_default_binding_profile() {
    // Create default profile if there is none
    auto ite = eka2l1::common::make_directory_iterator("bindings\\");
    if (!ite) {
        return;
    }

    eka2l1::common::dir_entry entry;

    int entry_count = 0;

    while (ite->next_entry(entry) >= 0) {
        if ((entry.name != ".") && (entry.name != ".."))
            entry_count++;
    }

    if (!entry_count) {
        emulator_state_.conf.current_keybind_profile = "default";
        make_default_keybind_profile(emulator_state_.conf.keybinds);

        emulator_state_.conf.serialize();
    }
}

void main_window::on_app_setting_changed() {
    eka2l1::epoc::screen *scr = get_current_active_screen();
    if (!scr || !scr->focus || !emulator_state_.app_settings) {
        return;
    }

    std::optional<eka2l1::akn_running_app_info> info = ::get_active_app_info(emulator_state_.symsys.get(), active_screen_number_);
    if (!info.has_value()) {
        return;
    }

    if (!settings_dialog_) {
        settings_dialog_ = new settings_dialog(this, emulator_state_.symsys.get(), emulator_state_.joystick_controller.get(),
            emulator_state_.app_settings.get(), emulator_state_.conf);
    }

    eka2l1::config::app_setting setting;
    setting.fps = scr->refresh_rate;
    setting.time_delay = info->associated_->get_time_delay();
    setting.child_inherit_setting = info->associated_->get_child_inherit_setting();
    setting.screen_rotation = scr->ui_rotation;
    setting.screen_upscale = scr->requested_ui_scale_factor;
    setting.screen_upscale_method = ((scr->flags_ & eka2l1::epoc::screen::FLAG_SCREEN_UPSCALE_FACTOR_LOCK) ? 1 : 0);
    setting.filter_shader_path = settings_dialog_->active_upscale_shader().toStdString();

    emulator_state_.app_settings->add_or_replace_setting(info->app_uid_, setting);
}

void main_window::refresh_mount_availbility() {
    ui_->action_mount_recent_dumps->setEnabled(false);
    ui_->menu_mount_game_card_dump->setEnabled(false);

    eka2l1::system *system = emulator_state_.symsys.get();
    if (system) {
        eka2l1::kernel_system *kern = system->get_kernel_system();
        if (kern) {
            if (kern->is_eka1()) {
                ui_->menu_mount_game_card_dump->setEnabled(true);
                ui_->action_mount_recent_dumps->setEnabled(true);
            }
        }
    }
}

void main_window::on_screen_current_group_change_callback() {
    QAction *buttons[] = {
        ui_->action_rotate_0,
        ui_->action_rotate_90,
        ui_->action_rotate_180,
        ui_->action_rotate_270
    };

    eka2l1::config::app_setting *setting = get_active_app_setting();
    eka2l1::epoc::screen *scr = get_current_active_screen();
    if (setting && scr) {
        scr->ui_rotation = setting->screen_rotation;
        buttons[setting->screen_rotation / 90]->setChecked(true);

        mode_change_screen(&emulator_state_, scr, 0);
    }
}

void main_window::on_another_rotation_triggered(QAction *action) {
    eka2l1::epoc::screen *scr = get_current_active_screen();
    if (!scr) {
        return;
    }

    if (action == ui_->action_rotate_0) {
        scr->ui_rotation = 0;
    } else if (action == ui_->action_rotate_90) {
        scr->ui_rotation = 90;
    } else if (action == ui_->action_rotate_180) {
        scr->ui_rotation = 180;
    } else if (action == ui_->action_rotate_270) {
        scr->ui_rotation = 270;
    }

    on_app_setting_changed();
    mode_change_screen(&emulator_state_, scr, 0);
}

void main_window::on_theme_change_requested(const QString &text) {
    if (text.count() == 1) {
        bool is_ok = false;
        int num = text.toInt(&is_ok);

        if (is_ok) {
            QCoreApplication *app = QApplication::instance();
            if (app) {
                if (num == 0) {
                    // Light mode, reset to default
                    application_.setStyleSheet("");
                } else {
                    QFile f(":assets/themes/dark/style.qss");

                    if (!f.exists()) {
                        QMessageBox::critical(this, tr("Load theme failed!"), tr("The Dark theme's style file can't be found!"));
                    } else {
                        f.open(QFile::ReadOnly | QFile::Text);
                        QTextStream ts(&f);
                        application_.setStyleSheet(ts.readAll());
                    }
                }
            }
        }
    }
}

void main_window::force_update_display_minimum_size() {
    if (!displayer_ || !displayer_->isVisible()) {
        return;
    }

    mode_change_screen(&emulator_state_, get_current_active_screen(), 0);
}

void main_window::on_window_title_setting_changed() {
    setWindowTitle(get_emulator_window_title());
}

void main_window::on_refresh_app_list_requested() {
    force_refresh_applist();
}

void main_window::save_ui_layouts() {
    QSettings settings;

    if (displayer_->isVisible()) {
        settings.setValue(LAST_EMULATED_DISPLAY_GEOMETRY_SETTING, saveGeometry());
        settings.setValue(LAST_EMULATED_DISPLAY_STATE, saveState());
    } else {
        settings.setValue(LAST_UI_WINDOW_GEOMETRY_SETTING, saveGeometry());
        settings.setValue(LAST_UI_WINDOW_STATE, saveState());
    }
}

void main_window::restore_ui_layouts() {
    QSettings settings;
    QVariant geo_variant;
    QVariant state_variant;

    if (displayer_->isVisible()) {
        geo_variant = settings.value(LAST_EMULATED_DISPLAY_GEOMETRY_SETTING);
        state_variant = settings.value(LAST_EMULATED_DISPLAY_STATE);
    } else {
        geo_variant = settings.value(LAST_UI_WINDOW_GEOMETRY_SETTING);
        state_variant = settings.value(LAST_UI_WINDOW_STATE);
    }

    if (geo_variant.isValid()) {
        restoreGeometry(geo_variant.toByteArray());
    }

    if (state_variant.isValid()) {
        restoreState(state_variant.toByteArray());
    }
}

bool main_window::input_dialog_open(const std::u16string &inital_text, const int max_length, eka2l1::drivers::ui::input_dialog_complete_callback complete_callback) {
    if (input_complete_callback_) {
        return false;
    }

    input_complete_callback_ = complete_callback;
    input_initial_text_ = inital_text;
    input_text_max_len_ = max_length;

    emit input_dialog_open_request();
    return true;
}

void main_window::input_dialog_close() {
    emit input_dialog_close_request();
}

void main_window::on_finished_text_input(const QString &text, const bool force_close) {
    if (input_complete_callback_) {
        input_complete_callback_(text.toStdU16String());
        input_complete_callback_ = nullptr;
    }

    if (!force_close) {
        input_dialog_->done(QDialog::Accepted);
    }

    input_dialog_ = nullptr;
}

void main_window::on_input_dialog_open_request() {
    if (input_dialog_) {
        LOG_ERROR(eka2l1::FRONTEND_UI, "Input dialog is already opened! We are aborting...");
        return;
    }

    input_dialog_ = new symbian_input_dialog(this);
    connect(input_dialog_, &symbian_input_dialog::finished_input, this, &main_window::on_finished_text_input);

    input_dialog_->open_for_input(QString::fromStdU16String(input_initial_text_), input_text_max_len_);
}

void main_window::on_input_dialog_close_request() {
    if (input_dialog_) {
        if (input_complete_callback_) {
            input_complete_callback_(input_dialog_->get_current_text().toStdU16String());
        }

        input_dialog_->done(QDialog::Accepted);
        input_dialog_ = nullptr;
    }

    input_complete_callback_ = nullptr;
}