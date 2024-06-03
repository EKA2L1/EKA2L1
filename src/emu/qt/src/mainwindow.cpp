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
#include "./links.h"

#include <qt/aboutdialog.h>
#include <qt/applistwidget.h>
#include <qt/btnetplay_friends_dialog.h>
#include <qt/btnet_dialog.h>
#include <qt/device_install_dialog.h>
#include <qt/displaywidget.h>
#include <qt/mainwindow.h>
#include <qt/symbian_input_dialog.h>
#include <qt/package_manager_dialog.h>
#include <qt/settings_dialog.h>
#include <qt/update_dialog.h>
#include <qt/update_notice_dialog.h>
#include <qt/launch_process_dialog.h>
#include <qt/state.h>
#include <qt/utils.h>
#include <qt/btnmap/editor_widget.h>
#include <qt/btnmap/executor.h>
#include <qt/custom_question_dialog.h>

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
#include <services/bluetooth/btman.h>
#include <services/fbs/fbs.h>
#include <services/ui/cap/oom_app.h>
#include <services/ui/skin/server.h>
#include <services/window/classes/wingroup.h>
#include <services/window/screen.h>
#include <services/window/window.h>

#include <j2me/interface.h>
#include <j2me/applist.h>

#include <QCheckBox>
#include <QDesktopWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFuture>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QLineEdit>
#include <QtConcurrent/QtConcurrent>

#include <stb_image.h>

#if EKA2L1_PLATFORM(WIN32)
#include <qt/winutils.h>
#endif

#include <qt/batch_install_dialog.h>

static constexpr const char *LAST_UI_WINDOW_GEOMETRY_SETTING = "lastWindowGeometry";
static constexpr const char *LAST_UI_WINDOW_STATE = "lastWindowState";
static constexpr const char *LAST_UI_WINDOW_MAXMIZED = "lastWindowMaximized";
static constexpr const char *LAST_EMULATED_DISPLAY_GEOMETRY_SETTING = "lastEmulatedDisplayGeometry";
static constexpr const char *LAST_EMULATED_DISPLAY_STATE = "lastEmulatedDisplayState";
static constexpr const char *LAST_EMULATED_DISPLAY_MAXIMIZED = "lastEmulatedDisplayMaximized";
static constexpr const char *LAST_PACKAGE_FOLDER_SETTING = "lastPackageFolder";
static constexpr const char *LAST_JAR_FOLDER_SETTING = "lastJarFolder";
static constexpr const char *LAST_MOUNT_FOLDER_SETTING = "lastMountFolder";
static constexpr const char *LAST_INSTALL_NGAGE_GAME_CARD_FOLDER_SETTING = "lastNGageGameCardFolder";
static constexpr const char *NO_DEVICE_INSTALL_DISABLE_NOF_SETTING = "disableNoDeviceInstallNotify";
static constexpr const char *NO_TOUCHSCREEN_DISABLE_WARN_SETTING = "disableNoTouchscreenWarn";
static constexpr const char *STRETCH_DISPLAY_SETTING = "stretchDisplay";
static constexpr int MOUSE_INACTIVITY_TIMEOUT_MS = 1500;

static void mode_change_screen(void *userdata, eka2l1::epoc::screen *scr, const int old_mode) {
    eka2l1::desktop::emulator *state_ptr = reinterpret_cast<eka2l1::desktop::emulator *>(userdata);
    if (!state_ptr) {
        return;
    }

    QSize new_minsize(scr->current_mode().size_original.x, scr->current_mode().size_original.y);
    if ((scr->ui_rotation % 180) != 0) {
        new_minsize = QSize(scr->current_mode().size_original.y, scr->current_mode().size_original.x);
    }

    QSettings settings;
    QVariant allow_true_size_variant = settings.value(TRUE_SIZE_RESIZE_SETTING_NAME, false);

    display_widget *widget = static_cast<display_widget *>(state_ptr->window);
    if (allow_true_size_variant.toBool()) {
        new_minsize /= widget->devicePixelRatioF();
    }

    auto desktop = QApplication::desktop();
    new_minsize.setWidth(std::min(new_minsize.width(), desktop->width()));
    new_minsize.setHeight(std::min(new_minsize.height(), desktop->height()));

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

    eka2l1::drivers::handle background_image = state_ptr->ui_main->get_background_image();
    builder.clear({ color_clear.z / 255.0f, color_clear.y / 255.0f, color_clear.x / 255.0f, color_clear.w / 255.0f, 0.0f, 0.0f }, eka2l1::drivers::draw_buffer_bit_color_buffer);
    if (background_image != 0) {
        eka2l1::rect draw_image_rect;
        draw_image_rect.size = swapchain_size;

        builder.set_feature(eka2l1::drivers::graphics_feature::blend, true);
        builder.blend_formula(eka2l1::drivers::blend_equation::add, eka2l1::drivers::blend_equation::add,
            eka2l1::drivers::blend_factor::frag_out_alpha, eka2l1::drivers::blend_factor::one_minus_frag_out_alpha,
            eka2l1::drivers::blend_factor::one, eka2l1::drivers::blend_factor::one_minus_frag_out_alpha);
        builder.set_brush_color_detail(eka2l1::vec4(255, 255, 255, eka2l1::common::clamp<int>(0, 255, state_ptr->conf.background_image_opacity)));
        builder.draw_bitmap(background_image, 0, draw_image_rect, eka2l1::rect(), eka2l1::vec2(0, 0), 0.0f, eka2l1::drivers::bitmap_draw_flag_use_brush);
        builder.set_feature(eka2l1::drivers::graphics_feature::blend, false);
    }

    auto &crr_mode = scr->current_mode();

    eka2l1::vec2 size = crr_mode.size;
    if ((scr->ui_rotation % 180) != 0) {
        std::swap(size.x, size.y);
    }

    float mult_x = scr->requested_ui_scale_factor > 0.0f ? scr->requested_ui_scale_factor : (static_cast<float>(window_width) / size.x);
    float mult_y = scr->requested_ui_scale_factor > 0.0f ? scr->requested_ui_scale_factor : (state.stretch_to_fill_display ? (static_cast<float>(window_height) / size.y) : mult_x);
    float width = size.x * mult_x;
    float height = size.y * mult_y;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    if (!state.stretch_to_fill_display) {
        if (height >= swapchain_size.y) {
            height = swapchain_size.y;
            mult_x = mult_y = height / size.y;
            width = size.x * mult_y;
        }
    }

    x = (swapchain_size.x - width) / 2;
    y = (swapchain_size.y - height) / 2;

    scr->set_native_scale_factor(state_ptr->graphics_driver.get(), mult_x, mult_y);
    scr->absolute_pos.x = static_cast<int>(x);
    scr->absolute_pos.y = static_cast<int>(y);

    dest.top = eka2l1::vec2(x, y);
    dest.size = eka2l1::vec2(width, height);

    src.size = crr_mode.size;
    src.size *= scr->display_scale_factor;

    eka2l1::drivers::advance_draw_pos_around_origin(dest, scr->ui_rotation);

    if (scr->ui_rotation % 180 != 0) {
        std::swap(dest.size.x, dest.size.y);
    }

    builder.set_texture_filter(scr->screen_texture, true, filter);
    builder.set_texture_filter(scr->screen_texture, false, filter);

    builder.draw_bitmap(scr->screen_texture, 0, dest, src, eka2l1::vec2(0, 0), static_cast<float>(scr->ui_rotation),
        (scr->flags_ & eka2l1::epoc::screen::FLAG_SCREEN_UPSCALE_FACTOR_LOCK) ? eka2l1::drivers::bitmap_draw_flag_use_upscale_shader : 0);

    if (state_ptr->ui_main) {
        builder.set_viewport(dest);
        state_ptr->ui_main->draw_enabled_overlay(state_ptr->graphics_driver.get(), builder, mult_x, mult_y);
    }

    builder.load_backup_state();

    state_ptr->present_status = -100;

    // Submit, present, and wait for the presenting
    builder.present(&state_ptr->present_status);

    eka2l1::drivers::command_list retrieved = builder.retrieve_command_list();
    state.graphics_driver->submit_command_list(retrieved);
}


main_window::main_window(QApplication &application, QWidget *parent, eka2l1::desktop::emulator &emulator_state
#if EKA2L1_PLATFORM(WIN32)
        , eka2l1::qt::window_transparent_manager *win_transparent_manager
#endif
    )
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
    , bt_netplay_dialog_(nullptr)
    , btnet_dialog_(nullptr)
    , editor_widget_(nullptr)
    , map_executor_(nullptr)
    , ui_(new Ui::main_window)
    , tray_icon_(new QSystemTrayIcon(this))
    , emu_icon_(":/assets/duck_tank.png")
    , applist_(nullptr)
    , displayer_(nullptr)
    , background_image_texture_(0)
    , rpc_(this)
#if EKA2L1_PLATFORM(WIN32)
    , win_transparent_manager_(win_transparent_manager)
#endif
    , should_maximized_(false)
    , ignore_sis_text_msg_(false)
    , auto_choose_sis_language_(false)
    , is_installing_batch_(false)
{
    ui_->setupUi(this);
    ui_->label_al_not_available->setVisible(false);

    eka2l1::kernel_system *kernel = emulator_state_.symsys->get_kernel_system();

    if (!emulator_state_.app_launch_from_command_line)
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

    auto *slider_whole = new QWidget(this);
    auto *slider_layout = new QHBoxLayout(slider_whole);

    auto *slider_icon = new QLabel(this);

    slider_icon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    slider_icon->setFixedHeight(current_device_label_->height());

    slider_whole->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    icon_size_slider_ = new QSlider(this);
    icon_size_slider_->setStyleSheet("QSlider:horizontal { min-height: 0px; }");
    icon_size_slider_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    icon_size_slider_->setOrientation(Qt::Horizontal);
    icon_size_slider_->setMinimum(applist_->get_icon_size_range().first);
    icon_size_slider_->setMaximum(applist_->get_icon_size_range().second);
    icon_size_slider_->setValue(applist_->get_icon_size());
    icon_size_slider_->setFixedHeight(current_device_label_->height());

    slider_layout->addWidget(slider_icon);
    slider_layout->addWidget(icon_size_slider_);
    slider_whole->setLayout(slider_layout);

    current_device_label_->setAlignment(Qt::AlignCenter);
    screen_status_label_->setAlignment(Qt::AlignCenter);

    ui_->status_bar->addPermanentWidget(slider_whole, 1);
    ui_->status_bar->addPermanentWidget(current_device_label_, 1);
    ui_->status_bar->addPermanentWidget(screen_status_label_, 1);

    icon_size_slider_widget_ = slider_whole;
    icon_symbol_label_ = slider_icon;

    screen_status_label_->setVisible(false);
    icon_size_slider_widget_->setVisible(true);

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
    emulator_state_.stretch_to_fill_display = settings.value(STRETCH_DISPLAY_SETTING, false).toBool();
    ui_->action_stretch_to_fill->setChecked(emulator_state_.stretch_to_fill_display);

    map_executor_ = new eka2l1::qt::btnmap::executor(nullptr, emulator_state_.symsys->get_ntimer());
    editor_widget_ = new editor_widget(this, map_executor_);
    addDockWidget(Qt::RightDockWidgetArea, editor_widget_);
    editor_widget_->setVisible(false);

    update_dialog *diag = new update_dialog(this);
    connect(diag, &update_dialog::exit_for_update_request, this, &main_window::on_exit_for_update_requested);

    diag->check_for_update(false);

    //update_notice_dialog::spawn(this);

    restore_ui_layouts();
    on_theme_change_requested(QString("%1").arg(settings.value(THEME_SETTING_NAME, 0).toInt()),
        settings.value(THEME_VARIANT_SETTING_NAME, theme_variant_acrylic).toInt());

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

    reprepare_touch_mappings();
    
    QColor default_color = settings.value(BACKGROUND_COLOR_DISPLAY_SETTING_NAME, QColor(0xD0, 0xD0, 0xD0)).value<QColor>();
    emulator_state_.conf.display_background_color = default_color.rgba();

    tray_icon_->setIcon(emu_icon_);
    tray_icon_->show();

    connect(ui_->action_about, &QAction::triggered, this, &main_window::on_about_triggered);
    connect(ui_->action_settings, &QAction::triggered, this, &main_window::on_settings_triggered);
    connect(ui_->action_package, &QAction::triggered, this, &main_window::on_package_install_clicked);
    connect(ui_->action_multiple_packages, &QAction::triggered, this, &main_window::on_multiple_package_install_clicked);
    connect(ui_->action_device, &QAction::triggered, this, &main_window::on_device_install_clicked);
    connect(ui_->action_ngage_card_game, &QAction::triggered, this, &main_window::on_install_ngage_card_game_clicked);
    connect(ui_->action_mount_game_card_folder, &QAction::triggered, this, &main_window::on_mount_card_clicked);
    connect(ui_->action_mount_game_card_zip, &QAction::triggered, this, &main_window::on_mount_zip_clicked);
    connect(ui_->action_fullscreen, &QAction::toggled, this, &main_window::on_fullscreen_toogled);
    connect(ui_->action_pause, &QAction::toggled, this, &main_window::on_pause_toggled);
    connect(ui_->action_restart, &QAction::triggered, this, &main_window::on_restart_requested);
    connect(ui_->action_package_manager, &QAction::triggered, this, &main_window::on_package_manager_triggered);
    connect(ui_->action_refresh_app_list, &QAction::triggered, this, &main_window::on_refresh_app_list_requested);
    connect(ui_->action_mod_netplay_friends, &QAction::triggered, this, &main_window::on_bt_netplay_mod_friends_clicked);
    connect(ui_->action_stretch_to_fill, &QAction::toggled, this, &main_window::on_action_stretch_to_fill_toggled);

    connect(&inactivity_timer_, &QTimer::timeout, this, &main_window::on_mouse_inactive);
    connect(rotate_group_, &QActionGroup::triggered, this, &main_window::on_another_rotation_triggered);
    connect(icon_size_slider_, &QSlider::valueChanged, this, &main_window::on_icon_size_slider_value_changed);

    connect(this, &main_window::progress_dialog_change, this, &main_window::on_progress_dialog_change, Qt::QueuedConnection);
    connect(this, &main_window::status_bar_update, this, &main_window::on_status_bar_update, Qt::QueuedConnection);
    connect(this, &main_window::install_ngage_game_name_available, this, &main_window::on_install_ngage_game_name_available, Qt::QueuedConnection);
    connect(this, &main_window::package_install_text_ask, this, &main_window::on_package_install_text_ask, Qt::BlockingQueuedConnection);
    connect(this, &main_window::package_install_language_choose, this, &main_window::on_package_install_language_choose, Qt::BlockingQueuedConnection);
    connect(this, &main_window::screen_focus_group_changed, this, &main_window::on_screen_current_group_change_callback, Qt::QueuedConnection);
    connect(this, &main_window::package_try_install_count_changed, this, &main_window::on_package_try_install_count_changed, Qt::QueuedConnection);

    connect(this, &main_window::input_dialog_delay_launch_asked, this, &main_window::on_input_dialog_delay_launch_asked);
    connect(this, &main_window::input_dialog_close_request, this, &main_window::on_input_dialog_close_request);
    connect(this, &main_window::question_dialog_open_request, this, &main_window::on_question_dialog_open_request, Qt::QueuedConnection);
    connect(this, &main_window::app_exited, this, &main_window::on_app_exited, Qt::QueuedConnection);

    connect(editor_widget_, &editor_widget::editor_hidden, this, &main_window::on_mapping_editor_hidden);

    setAcceptDrops(true);
}

void main_window::setup_app_list(const bool load_now) {
    if (applist_)
        return;

    eka2l1::system *system = emulator_state_.symsys.get();
    eka2l1::kernel_system *kernel = system->get_kernel_system();
    if (kernel) {
        const std::string al_server_name = eka2l1::get_app_list_server_name_by_epocver(kernel->get_epoc_version());
        const std::string fbs_server_name = eka2l1::epoc::get_fbs_server_name_by_epocver(kernel->get_epoc_version());

        eka2l1::applist_server *al_serv = reinterpret_cast<eka2l1::applist_server *>(kernel->get_by_name<eka2l1::service::server>(al_server_name));
        eka2l1::fbs_server *fbs_serv = reinterpret_cast<eka2l1::fbs_server *>(kernel->get_by_name<eka2l1::service::server>(fbs_server_name));
        eka2l1::akn_skin_server *skin_serv = reinterpret_cast<eka2l1::akn_skin_server *>(kernel->get_by_name<eka2l1::service::server>(eka2l1::AKN_SKIN_SERVER_NAME));

        if (al_serv && fbs_serv) {
            applist_ = new applist_widget(this, al_serv, skin_serv, fbs_serv, system->get_io_system(), system->get_j2me_applist(), emulator_state_.conf, emulator_state_.conf.hide_system_apps, true);
            ui_->layout_main->addWidget(applist_);

            connect(applist_, &applist_widget::app_launch, this, &main_window::on_app_clicked);
            connect(applist_, &applist_widget::device_change_request, this, &main_window::on_device_set_requested, Qt::QueuedConnection);

            applist_->update_devices(system->get_device_manager());
            if (load_now) {
                applist_->request_reload();
            }
        }
    }

    if (!applist_) {
        QSettings settings;
        QVariant no_notify_install = settings.value(NO_DEVICE_INSTALL_DISABLE_NOF_SETTING);

        if (!no_notify_install.isValid() || !no_notify_install.toBool()) {
            const QMessageBox::StandardButton result = make_dialog_with_checkbox_and_choices(
                tr("No device installed"), tr("You have not installed any device. Please visit <a href=\"" WIKI_LINK "\">EKA2L1 wiki</a> to get started or install a device."),
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

#if ENABLE_DISCORD_RICH_PRESENCE
    rpc_.update(tr("Browsing").toStdString(), tr("App list").toStdString(), true);
#endif
}

void main_window::setup_package_installer_ui_hooks() {
    eka2l1::system *system = emulator_state_.symsys.get();
    eka2l1::manager::packages *pkgmngr = system->get_packages();

    pkgmngr->show_text = [this](const char *text, const bool one_button) -> bool {
        // We only have one receiver, so it's ok ;)
        if (ignore_sis_text_msg_) {
            return one_button ? 0 : 1;
        }

        return emit package_install_text_ask(text, one_button);
    };

    pkgmngr->choose_lang = [this](const int *languages, const int language_count) -> int {
        if (auto_choose_sis_language_) {
            auto def_lang = language::en;
            if (emulator_state_.symsys) {
                def_lang = emulator_state_.symsys->get_system_language();
            }

            if (std::find(languages, languages + language_count, static_cast<int>(def_lang)) != languages + language_count) {
                return static_cast<int>(def_lang);
            } else {
                return languages[0];
            }
        }

        return emit package_install_language_choose(languages, language_count);
    };
}

void main_window::reprepare_touch_mappings() {
    map_executor_->set_window_server(::get_window_server_through_system(emulator_state_.symsys.get()));
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

    delete editor_widget_;
    delete recent_mount_folder_menu_;
    delete recent_mount_clear_;

    for (std::size_t i = 0; i < MAX_RECENT_ENTRIES; i++) {
        delete recent_mount_folder_actions_[i];
    }

    delete current_device_label_;
    delete screen_status_label_;

    delete rotate_group_;
    delete map_executor_;
    delete tray_icon_;

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
        connect(settings_dialog_.data(), &settings_dialog::hide_system_apps_changed, this, &main_window::on_hide_system_apps_changed, Qt::DirectConnection);
        connect(settings_dialog_.data(), &settings_dialog::theme_variant_combo_init, this, &main_window::on_theme_variant_combo_init);
        connect(settings_dialog_.data(), &settings_dialog::skin_changed, this, &main_window::on_skin_changed);

        connect(this, &main_window::app_launching, settings_dialog_.data(), &settings_dialog::on_app_launching);
        connect(this, &main_window::controller_button_press, settings_dialog_.data(), &settings_dialog::on_controller_button_press);
        connect(this, &main_window::screen_focus_group_changed, settings_dialog_.data(), &settings_dialog::refresh_app_configuration_details);
        connect(this, &main_window::restart_finished, settings_dialog_.data(), &settings_dialog::on_restart_finished_from_main);

        settings_dialog_->set_active_tab(0);
        settings_dialog_->show();
    } else {
        settings_dialog_->raise();
    }
}

void main_window::force_refresh_applist() {
    // Try to refersh app lists
    if (applist_ && applist_->lister_->rescan_registries(applist_->io_)) {
        applist_->request_reload(false);
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
    if (bt_netplay_dialog_) {
        bt_netplay_dialog_->close();
        bt_netplay_dialog_ = nullptr;
    }

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

    if (index >= 0) {
        emulator_state_.conf.device = index;
    }

    emulator_state_.conf.serialize();

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
    refresh_current_device_label();
    refresh_mount_availbility();
    reprepare_touch_mappings();
    screen_status_label_->clear();

    ui_->action_pause->setEnabled(false);
    ui_->action_restart->setEnabled(false);
    ui_->action_pause->setChecked(false);

    screen_status_label_->setVisible(false);
    icon_size_slider_widget_->setVisible(true);

    emit restart_finished();

    setup_app_list(true);
}

void main_window::on_restart_requested() {
    save_ui_layouts();

    ui_->status_bar->show();
    ui_->menu_bar->setVisible(true);

    inactivity_timer_.stop();
    setCursor(Qt::ArrowCursor);

    displayer_->setMouseTracking(false);
    displayer_->removeEventFilter(this);

    on_device_set_requested(-1);
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

        emulator_state_.symsys->mount(drive_c, drive_media::physical, eka2l1::add_path(emulator_state_.conf.storage, "/drives/c/"), io_attrib_internal);
        emulator_state_.symsys->mount(drive_d, drive_media::physical, eka2l1::add_path(emulator_state_.conf.storage, "/drives/d/"), io_attrib_internal);
        emulator_state_.symsys->mount(drive_e, drive_media::physical, eka2l1::add_path(emulator_state_.conf.storage, "/drives/e/"), io_attrib_removeable);

        // Set and wait for reinitialization
        emulator_state_.init_event.set();
        emulator_state_.init_event.wait();

        refresh_current_device_label();
        reprepare_touch_mappings();
        setup_app_list(true);
    } else {
        if (emulator_state_.symsys) {
            applist_->update_devices(emulator_state_.symsys->get_device_manager());
        }
    }
}

void main_window::on_device_install_clicked() {
    device_install_dialog *install_diag = new device_install_dialog(this, emulator_state_.symsys->get_device_manager(), emulator_state_.conf);
    connect(install_diag, &device_install_dialog::new_device_added, this, &main_window::on_new_device_added);

    install_diag->show();
}

void main_window::on_install_ngage_game_name_available(QString name) {
    ngage_game_installing_name_ = name;
    if (current_progress_dialog_) {
        current_progress_dialog_->setLabelText(tr("Installing <b>%1</b>").arg(name));
    }
}

void main_window::on_install_ngage_card_game_clicked() {
    QSettings settings;
    QString last_install_dir;

    QVariant last_install_dir_variant = settings.value(LAST_INSTALL_NGAGE_GAME_CARD_FOLDER_SETTING);
    if (last_install_dir_variant.isValid()) {
        last_install_dir = last_install_dir_variant.toString();
    }

    QString install_folder = QFileDialog::getExistingDirectory(this, tr("Choose the N-Gage game card folder"), last_install_dir);
    if (!install_folder.isEmpty()) {
        settings.setValue(LAST_INSTALL_NGAGE_GAME_CARD_FOLDER_SETTING, install_folder);

        current_progress_dialog_ = new QProgressDialog(QString{}, "", 0, 100, this);
        ngage_game_installing_name_ = "";

        current_progress_dialog_->setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::WindowMaximizeButtonHint & ~Qt::WindowCloseButtonHint);
        current_progress_dialog_->setWindowTitle(tr("Installing N-Gage game..."));
        current_progress_dialog_->setWindowModality(Qt::ApplicationModal);
        current_progress_dialog_->setAttribute(Qt::WA_DeleteOnClose, true);
        current_progress_dialog_->setCancelButton(nullptr);
        current_progress_dialog_->show();

        QFuture<eka2l1::ngage_game_card_install_error> install_future = QtConcurrent::run([this, install_folder]() -> eka2l1::ngage_game_card_install_error {
            return emulator_state_.symsys->install_ngage_game_card(install_folder.toStdString(), [this](const std::string &game_name) {
                emit install_ngage_game_name_available(QString::fromStdString(game_name));
            }, [this](const std::size_t done, const std::size_t total) {
                emit progress_dialog_change(done, total);
            });
        });

        while (!install_future.isFinished()) {
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        current_progress_dialog_->close();
        const eka2l1::ngage_game_card_install_error install_future_result = install_future.result();

        if (install_future_result == eka2l1::ngage_game_card_install_success) {
            QMessageBox::information(this, tr("Install success!"), tr("Successfully install N-Gage card game: <b>%1</b>").arg(ngage_game_installing_name_));
            force_refresh_applist();
        } else {
            QString error_str;
            switch (install_future_result) {
            case eka2l1::ngage_game_card_no_game_data_folder:
                error_str = tr("Can't find the game data folder!");
                break;

            case eka2l1::ngage_game_card_more_than_one_data_folder:
                error_str = tr("There is more than one game in the given card game folder!");
                break;

            case eka2l1::ngage_game_card_no_game_registeration_info:
                error_str = tr("The game information file does not exist in the card game folder!");
                break;

            case eka2l1::ngage_game_card_registeration_corrupted:
                error_str = tr("The game information file has been corrupted. Please check your data's validity!");
                break;

            default:
                error_str = tr("General error occured...");
                break;
            }

            if (ngage_game_installing_name_.isEmpty()) {
                QMessageBox::critical(this, tr("Install failed"), tr("Installation failed with error:\n  -%1").arg(error_str));
            } else {
                QMessageBox::critical(this, tr("Install failed"), tr("Installation of %1% failed with error:\n  -%2").arg(ngage_game_installing_name_, error_str));
            }
        }
    }
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

        if (should_maximized_) {
            showMaximized();
        } else {
            showNormal();
        }
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

    auto do_check_and_assign_mmc_id = [mount_path, this]() {
        std::optional<std::string> mmc_id_found = get_mmc_id_from_path(mount_path.toStdString());
        if (mmc_id_found.has_value()) {
            emulator_state_.conf.current_mmc_id = mmc_id_found.value();
            tray_icon_->showMessage(tr("MMC ID found"), tr("MMC ID found in folder name. Using it as current MMC ID."), emu_icon_, 2000);
        }
    };

    eka2l1::io_system *io = emulator_state_.symsys->get_io_system();

    const std::string path_ext = eka2l1::path_extension(mount_path.toStdString());
    if (!eka2l1::common::is_dir(mount_path.toStdString()) && (eka2l1::common::compare_ignore_case(path_ext.c_str(), ".zip") == 0)) {
        io->unmount(drive_e);
        current_progress_dialog_ = new QProgressDialog(QString{}, tr("Cancel"), 0, 100, this);

        current_progress_dialog_->setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::WindowMaximizeButtonHint & ~Qt::WindowCloseButtonHint);
        current_progress_dialog_->setWindowTitle(tr("Extracting game dump files"));
        current_progress_dialog_->setWindowModality(Qt::ApplicationModal);
        current_progress_dialog_->setAttribute(Qt::WA_DeleteOnClose, true);
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
                break;
            }

            default: {
                do_check_and_assign_mmc_id();
                break;
            }
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

        do_check_and_assign_mmc_id();
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
        applist_->request_reload(false);
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

        editor_widget_->hear_active_app(system);
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

    screen_status_label_->setVisible(true);
    icon_size_slider_widget_->setVisible(false);

    before_margins_ = ui_->layout_centralwidget->contentsMargins();
    on_fullscreen_toogled(ui_->action_fullscreen->isChecked());

    displayer_->setMouseTracking(true);
    displayer_->installEventFilter(this);
}

void main_window::on_app_exited(eka2l1::kernel::process *target_proc) {
    bool shown_msg = false;

    if (target_proc->get_exit_type() == eka2l1::kernel::entity_exit_type::kill) {
        if ((target_proc->get_exit_reason() == 0) || (target_proc->get_exit_category() == u"None")) {
            tray_icon_->showMessage(tr("Application exited"), tr("The application exited normally"), emu_icon_, 1500);
            shown_msg = true;
        }
    }

    if (!shown_msg) {
        QString category_exit = QString::fromStdU16String(target_proc->get_exit_category());
        int reason_exit = target_proc->get_exit_reason();

        switch (target_proc->get_exit_type()) {
        case eka2l1::kernel::entity_exit_type::kill:
            tray_icon_->showMessage(tr("Application exited"), tr("The application was killed with code: %1/%2").arg(category_exit).arg(reason_exit), emu_icon_, 1500);
            break;

        case eka2l1::kernel::entity_exit_type::panic:
            tray_icon_->showMessage(tr("Application exited"), tr("The application panicked with code: %1/%2").arg(category_exit).arg(reason_exit), emu_icon_, 1500);
            break;

        case eka2l1::kernel::entity_exit_type::terminate:
            tray_icon_->showMessage(tr("Application exited"), tr("The application terminated with code: %1/%2").arg(category_exit).arg(reason_exit), emu_icon_, 1500);
            break;

        default:
            break;
        }
    }

    if (emulator_state_.app_launch_from_command_line) {
        close();
    } else {
        on_restart_requested();
    }
}

std::function<void(eka2l1::kernel::process *)> main_window::get_process_exit_callback() {
    return [this](eka2l1::kernel::process *proc) { emit app_exited(proc); };
}

void main_window::set_discord_presence_current_playing(const std::string &name) {
#if ENABLE_DISCORD_RICH_PRESENCE
        std::string state = tr("Playing %1").arg(QString::fromStdString(name)).toStdString();
        std::string detail = tr("In game").toStdString();

        rpc_.update(state, detail, true);
#endif
}

void main_window::on_app_clicked(applist_widget_item *item) {
    eka2l1::config::app_setting app_setting;

    // NOTE: Use this feature in the future! Ha!
    app_setting.force_resolution = eka2l1::vec2(0, 0);

    emulator_state_.symsys->set_launch_app_setting(app_setting);

    if (!active_screen_draw_callback_) {
        setup_screen_draw();
    }

    bool launch_ok = false;
    if (item->is_j2me_) {
        launch_ok = eka2l1::j2me::launch(emulator_state_.symsys.get(), static_cast<std::uint32_t>(item->registry_index_), get_process_exit_callback());
    } else  {
        launch_ok = applist_->launch_from_widget_item(item, get_process_exit_callback());
    }

    if (launch_ok) {
        set_discord_presence_current_playing(applist_->get_app_name_from_widget_item(item));
        switch_to_game_display_mode();
        emit app_launching();
    } else {
        QMessageBox::critical(this, tr("Launch failed"), tr("Fail to launch the selected application!"));
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

    if (is_installing_batch_) {
        current_progress_dialog_->setLabelText(tr("Package %1/%2 - %3%").arg(total_try_installed_).arg(total_to_install_).arg(
            static_cast<float>(now * 100.0f) / static_cast<float>(total), 0, 'f', 2));
    }
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
    auto_choose_sis_language_ = false;
    ignore_sis_text_msg_ = false;

    if (!package_file_path.isEmpty()) {
        eka2l1::manager::packages *pkgmngr = emulator_state_.symsys->get_packages();
        if (pkgmngr) {
            current_progress_dialog_ = new QProgressDialog(QString{}, tr("Cancel"), 0, 100, this);

            current_progress_dialog_->setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::WindowMaximizeButtonHint & ~Qt::WindowCloseButtonHint);
            current_progress_dialog_->setWindowTitle(tr("Installing package progress"));
            current_progress_dialog_->setAttribute(Qt::WA_DeleteOnClose, true);
            current_progress_dialog_->show();

            drive_number install_drive = drive_e;
            if (emulator_state_.symsys->is_s80_device_active()) {
                install_drive = drive_d;
            }

            QFuture<eka2l1::package::installation_result> install_future = QtConcurrent::run([this, pkgmngr, package_file_path, install_drive]() {
                return pkgmngr->install_package(
                    package_file_path.toStdU16String(),
                    install_drive,
                    [this](const std::size_t done, const std::size_t total) { emit progress_dialog_change(done, total); }, [this] { return current_progress_dialog_->wasCanceled(); });
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

void main_window::spawn_multiple_install_packages(QList<QString> files) {
    auto batch_diag = new batch_install_dialog(this, files);
    connect(batch_diag, &batch_install_dialog::install_multiple, this, &main_window::on_batch_install_request_start, Qt::QueuedConnection);

    batch_diag->show();
}

void main_window::on_batch_install_request_start(bool should_ignore_sis_msg, bool should_auto_choose_language,
    QList<QString> files) {
    ignore_sis_text_msg_ = should_ignore_sis_msg;
    auto_choose_sis_language_ = should_auto_choose_language;

    eka2l1::manager::packages *pkgmngr = emulator_state_.symsys->get_packages();
    if (pkgmngr) {
        current_progress_dialog_ = new QProgressDialog(QString{}, tr("Cancel"), 0, 100, this);

        current_progress_dialog_->setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::WindowMaximizeButtonHint & ~Qt::WindowCloseButtonHint);
        current_progress_dialog_->setWindowTitle(tr("Installing packages progress"));
        current_progress_dialog_->setAttribute(Qt::WA_DeleteOnClose, true);
        current_progress_dialog_->setMinimumWidth(this->width() / 2);
        current_progress_dialog_->setModal(true);
        current_progress_dialog_->show();

        drive_number install_drive = drive_e;
        if (emulator_state_.symsys->is_s80_device_active()) {
            install_drive = drive_d;
        }

        using install_abnormal_map = std::unordered_map<std::string, eka2l1::package::installation_result>;

        std::atomic_size_t total_success_install = 0;
        is_installing_batch_ = true;

        QFuture<install_abnormal_map> install_future = QtConcurrent::run([this, pkgmngr, &files, install_drive, &total_success_install]() {
            float percentage_each = 100.0f / static_cast<float>(files.size());
            install_abnormal_map abnormal_map;

            for (auto i = 0; i < files.size(); i++) {
                if (current_progress_dialog_->wasCanceled()) {
                    return abnormal_map;
                }

                auto res = pkgmngr->install_package(
                    files[i].toStdU16String(),
                    install_drive,
                    [this, percentage_each, i](const std::size_t done, const std::size_t total) {
                        float percentage_in_single = static_cast<float>(done) / static_cast<float>(total);
                        float percentage_total = percentage_each * (static_cast<float>(i) + percentage_in_single);
                        emit progress_dialog_change(static_cast<std::size_t>(percentage_total), 100); },
                    [this] { return current_progress_dialog_->wasCanceled(); });

                if (res != eka2l1::package::installation_result_success) {
                    abnormal_map.emplace(files[i].toStdString(), res);
                } else {
                    total_success_install = total_success_install + 1;
                }

                emit package_try_install_count_changed(i, files.size());
            }

            return abnormal_map;
        });

        while (!install_future.isFinished()) {
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        current_progress_dialog_->close();
        is_installing_batch_ = false;

        auto result = install_future.result();

        if (result.empty()) {
            if (total_success_install == files.size()) {
                QMessageBox::information(this, tr("Installation success"), tr("All packages have been successfully installed"));
            } else {
                QMessageBox::information(this, tr("Installation success"), tr("%1/%2 packages have been successfully installed").arg(total_success_install).arg(files.size()));
            }
        } else {
            QString error_str;

            for (const auto &[file, res] : result) {
            switch (res) {
            case eka2l1::package::installation_result_invalid:
                error_str += tr("Fail to install package at path: %1. Ensure the path points to a valid SIS/SISX file.\n").arg(QString::fromStdString(file));
                break;

            case eka2l1::package::installation_result_aborted:
                error_str += tr("Installation of package at path: %1 has been canceled.\n").arg(QString::fromStdString(file));
                break;

            default:
                break;
            }
            }

            auto box = new QMessageBox(this);

            box->setWindowTitle(tr("Installation info"));

            if (total_success_install == 0) {
                box->setText(tr("%1/%2 packages failed to install!").arg(result.size()).arg(files.size()));
            } else {
                box->setText(tr("%1/%2 packages successfully installed. %3/%4 packages failed to install.").arg(
                    total_success_install).arg(files.size()).arg(result.size()).arg(files.size()));
            }

            box->setAttribute(Qt::WA_DeleteOnClose);
            box->setStandardButtons(QMessageBox::Ok);
            box->setDetailedText(error_str);

            box->show();
        }

        if (total_success_install > 0) {
            force_refresh_applist();
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

void main_window::on_multiple_package_install_clicked() {
    QSettings settings;
    QString last_install_dir;

    QVariant last_install_dir_variant = settings.value(LAST_PACKAGE_FOLDER_SETTING);
    if (last_install_dir_variant.isValid()) {
            last_install_dir = last_install_dir_variant.toString();
    }

    QStringList package_file_paths = QFileDialog::getOpenFileNames(this, tr("Choose the files to install"), last_install_dir, tr("SIS file (*.sis *.sisx)"));
    if (!package_file_paths.isEmpty()) {
        settings.setValue(LAST_PACKAGE_FOLDER_SETTING, QFileInfo(package_file_paths[0]).absoluteDir().path());
    }

    QList<QString> file_paths;
    for (const QString &package_file_path : package_file_paths) {
        file_paths.push_back(package_file_path);
    }

    spawn_multiple_install_packages(file_paths);
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
        QString local_path = list_url[0].toLocalFile();
        if (local_path.endsWith(".sis") || local_path.endsWith(".sisx")) {
            event->acceptProposedAction();
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
    } else {
        QList<QString> file_paths;

        for (const QUrl &url : list_url) {
            auto local_file_path = url.toLocalFile();

            if (local_file_path.endsWith(".sis") || local_file_path.endsWith(".sisx")) {
                file_paths.push_back(local_file_path);
            }
        }

        spawn_multiple_install_packages(file_paths);
    }
}

void main_window::closeEvent(QCloseEvent *event) {
    save_ui_layouts();
}

bool main_window::controller_event_handler(eka2l1::drivers::input_event &event) {
    emit controller_button_press(event);
    if (displayer_ && displayer_->isVisible() && displayer_->hasFocus()) {
        if (editor_widget_->isVisible() && editor_widget_->handle_controller_event(event)) {
            return false;
        }
        if (event.button_.state_ == eka2l1::drivers::button_state::joy) {
            return !map_executor_->execute((static_cast<std::uint64_t>(eka2l1::qt::btnmap::MAP_TYPE_GAMEPAD) << 32) | event.button_.button_,
                                          event.button_.axis_[0], event.button_.axis_[1]);
        }
        return !map_executor_->execute((static_cast<std::uint64_t>(eka2l1::qt::btnmap::MAP_TYPE_GAMEPAD) << 32) | event.button_.button_,
                                      event.button_.state_ == eka2l1::drivers::button_state::pressed);
    }
    return false;
}

void main_window::make_default_binding_profile() {
    // Create default profile if there is none
    auto ite = eka2l1::common::make_directory_iterator("bindings\\", "");
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
    ui_->action_ngage_card_game->setEnabled(false);
    ui_->action_jar->setEnabled(false);

    eka2l1::system *system = emulator_state_.symsys.get();
    if (system) {
        eka2l1::kernel_system *kern = system->get_kernel_system();
        if (kern) {
            if (kern->is_eka1()) {
                ui_->menu_mount_game_card_dump->setEnabled(true);
                ui_->action_mount_recent_dumps->setEnabled(true);
                ui_->action_ngage_card_game->setEnabled(true);
            }

            ui_->action_jar->setEnabled(true);
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

void main_window::on_theme_change_requested(const QString &text, const int variant) {
    if (text.count() == 1) {
        bool is_ok = false;
        int num = text.toInt(&is_ok);

        if (is_ok) {
            QCoreApplication *app = QApplication::instance();
            if (app) {
                std::unique_ptr<QFile> f;
                bool need_load_more_stylesheet = true;

#if EKA2L1_PLATFORM(WIN32)
                if (win_transparent_manager_->is_supported()) {
                    if (variant == theme_variant_classic) {
                        if (!win_transparent_manager_->is_enabled()) {
                            if (num == 0) {
                                application_.setStyleSheet("");
                                need_load_more_stylesheet = false;
                            } else {
                                if (num == 2) {
                                    if (win_transparent_manager_->is_supported()) {
                                        num = (win_transparent_manager_->is_system_dark_mode()) ? 1 : 0;
                                    } else {
                                        num = 1;
                                    }
                                }

                                f = std::make_unique<QFile>(":assets/themes/dark/style.qss");
                            }
                        } else {
                            QMessageBox::information(this, tr("Relaunch needed"), tr("Classic theme variant will be applied on the next launch of the emulator."));
                            return;
                        }
                    } else {
                        if (num == 2) {
                            if (win_transparent_manager_->is_supported()) {
                                num = (win_transparent_manager_->is_system_dark_mode()) ? 1 : 0;
                            } else {
                                num = 1;
                            }
                        }

                        f = std::make_unique<QFile>(num == 0 ? ":assets/themes/QtWin11/light.qss" : ":assets/themes/QtWin11/dark.qss");

                        win_transparent_manager_->set_use_acrylic(variant == theme_variant_acrylic);
                        win_transparent_manager_->set_dark_theme(num == 1);
                    }
                } else {
#endif
                    if (num > 1) {
                        num = 1;
                    }

                    if (num == 0) {
                        application_.setStyleSheet("");
                        need_load_more_stylesheet = false;
                    } else {
                        f = std::make_unique<QFile>(":assets/themes/dark/style.qss");
                    }

#if EKA2L1_PLATFORM(WIN32)
                }
#endif

                if (need_load_more_stylesheet) {
                    if (!f || !f->exists()) {
                        QMessageBox::critical(this, tr("Load theme failed!"), tr("The Dark theme's style file can't be found!"));
                    } else {
                        f->open(QFile::ReadOnly | QFile::Text);
                        QTextStream ts(f.get());

                        QString styleSheet = ts.readAll();

#if EKA2L1_PLATFORM(WIN32)
                        if (win_transparent_manager_->is_supported() && win_transparent_manager_->is_enabled()) {
                            auto accent_color = win_transparent_manager_->get_accent_color();
                            auto rgb_string = QString("rgb(%1, %2, %3)").arg(accent_color.red()).arg(accent_color.green()).arg(accent_color.blue());

                            styleSheet = styleSheet.arg(rgb_string);
                        }
#endif

                        application_.setStyleSheet(styleSheet);
                    }
                }

                QPixmap pixmap = QIcon(num == 1 ? ":/assets/icon_symbol_dark.svg" : ":/assets/icon_symbol.svg").
                                 pixmap(QSize(icon_symbol_label_->height(), icon_symbol_label_->height()));

                icon_symbol_label_->setPixmap(pixmap);
            }
        }
    }
}

void main_window::on_theme_variant_combo_init(QWidget *container, QComboBox *combo) {
    container->setVisible(false);

#if EKA2L1_PLATFORM(WIN32)
    if (win_transparent_manager_->is_supported()) {
        container->setVisible(true);

        QSettings settings;
        QVariant theme_variant = settings.value(THEME_VARIANT_SETTING_NAME, "1");

        if (theme_variant.isValid()) {
            combo->setCurrentIndex(theme_variant.toInt());
        }
    } else {
        combo->setCurrentIndex(0);
    }
#else
    combo->setCurrentIndex(0);
#endif
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

void main_window::on_skin_changed() {
    force_refresh_applist();
}

void main_window::save_ui_layouts() {
    QSettings settings;

    bool is_fullscreen = ui_->action_fullscreen->isChecked();

    if (displayer_->isVisible()) {
        settings.setValue(LAST_EMULATED_DISPLAY_GEOMETRY_SETTING, saveGeometry());
        settings.setValue(LAST_EMULATED_DISPLAY_STATE, saveState());
        settings.setValue(LAST_EMULATED_DISPLAY_MAXIMIZED, is_fullscreen ? should_maximized_ : isMaximized());
    } else {
        settings.setValue(LAST_UI_WINDOW_GEOMETRY_SETTING, saveGeometry());
        settings.setValue(LAST_UI_WINDOW_STATE, saveState());
        settings.setValue(LAST_UI_WINDOW_MAXMIZED, is_fullscreen ? should_maximized_ : isMaximized());
    }
}

void main_window::restore_ui_layouts() {
    QSettings settings;
    QVariant geo_variant;
    QVariant state_variant;
    bool maximized;

    if (displayer_->isVisible()) {
        geo_variant = settings.value(LAST_EMULATED_DISPLAY_GEOMETRY_SETTING);
        state_variant = settings.value(LAST_EMULATED_DISPLAY_STATE);
        maximized = settings.value(LAST_EMULATED_DISPLAY_MAXIMIZED, false).toBool();
    } else {
        geo_variant = settings.value(LAST_UI_WINDOW_GEOMETRY_SETTING);
        state_variant = settings.value(LAST_UI_WINDOW_STATE);
        maximized = settings.value(LAST_UI_WINDOW_MAXMIZED, false).toBool();
    }

    const bool was_visible = editor_widget_->isVisible();

    if (geo_variant.isValid()) {
        restoreGeometry(geo_variant.toByteArray());
    }

    if (state_variant.isValid()) {
        restoreState(state_variant.toByteArray());
    }

    if (maximized) {
        showMaximized();
    } else {
        showNormal();
    }

    should_maximized_ = maximized;

    if (!was_visible)
        editor_widget_->setVisible(false);
}

bool main_window::input_dialog_open(const std::u16string &inital_text, const int max_length, eka2l1::drivers::ui::input_dialog_complete_callback complete_callback) {
    if (input_complete_callback_) {
        return false;
    }

    input_complete_callback_ = complete_callback;
    input_initial_text_ = inital_text;
    input_text_max_len_ = max_length;

    emit input_dialog_delay_launch_asked();
    return true;
}

void main_window::input_dialog_close() {
    emit input_dialog_close_request();
}

void main_window::on_input_dialog_delay_launch_asked() {
    // Give it a bit of time so that the Select (Middle/5) button release can be submitted
    QTimer::singleShot(200, this, &main_window::on_input_dialog_open_request);
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

void main_window::on_hide_system_apps_changed() {
    if (applist_) {
        applist_->set_hide_system_apps(emulator_state_.conf.hide_system_apps);
    }
}

void main_window::spawn_btnet_friends_dialog(bool is_from_configure) {
    if (!bt_netplay_dialog_) {
        QMessageBox::StandardButton result = QMessageBox::warning(this, tr("Continue to modify friends' IP addresses"),
                                                                  tr("This dialog will show all stored IP addresses, which has the potential of revealing others' personal information.<br>"
                                                                     "Do you wish to continue?"), QMessageBox::Yes | QMessageBox::No);

        if (result == QMessageBox::Yes) {
            eka2l1::btman_server *btman_serv = nullptr;
            if (emulator_state_.symsys) {
                eka2l1::kernel_system *kernel = emulator_state_.symsys->get_kernel_system();
                const std::string btman_server_name = eka2l1::get_btman_server_name_by_epocver(kernel->get_epoc_version());
                btman_serv = kernel->get_by_name<eka2l1::btman_server>(btman_server_name);
            }
            if (btman_serv) {
                // TODO: Check for other type of midman (real bluetooth for example!)
                bt_netplay_dialog_ = new btnetplay_friends_dialog(is_from_configure ? static_cast<QWidget*>(btnet_dialog_) : static_cast<QWidget*>(this), reinterpret_cast<eka2l1::epoc::bt::midman_inet*>(btman_serv->get_midman()), emulator_state_.conf);
                connect(bt_netplay_dialog_, &QDialog::finished, this, &main_window::on_btnetplay_friends_dialog_finished);

                bt_netplay_dialog_->show();
            } else {
                QMessageBox::critical(this, tr("Bluetooth manager service not available!"), tr("The emulated device does not have a bluetooth manager service! Bluetooth is not supported."));
            }
        }
    } else {
        bt_netplay_dialog_->setFocus();
    }
}

void main_window::on_bt_netplay_mod_friends_clicked() {
    spawn_btnet_friends_dialog(false);
}

void main_window::on_btnetplay_friends_dialog_finished(int status) {
    bt_netplay_dialog_ = nullptr;
}

void main_window::draw_enabled_overlay(eka2l1::drivers::graphics_driver *driver, eka2l1::drivers::graphics_command_builder &builder, const float scale_factor_x, const float scale_factor_y) {
    if (!editor_widget_->isVisible()) {
        return;
    }
    editor_widget_->draw(driver, builder, { scale_factor_x, scale_factor_y });
}

bool main_window::deliver_overlay_mouse_event(const eka2l1::vec3 &pos, const int button_id, const int action_id,
    const int mouse_id) {
    if (!emulator_state_.winserv) {
        return false;
    }

    if (!editor_widget_->isVisible()) {
        return false;
    }

    if (mouse_id == -1) {
        return false;
    }

    eka2l1::epoc::screen *scr = emulator_state_.winserv->get_screens();
    eka2l1::vec3 readjusted_pos = (pos - eka2l1::vec3(scr->absolute_pos, 0));

    readjusted_pos.x = (readjusted_pos.x < 0 ? 0 : readjusted_pos.x);
    readjusted_pos.y = (readjusted_pos.y < 0 ? 0 : readjusted_pos.y);
    readjusted_pos.z = (readjusted_pos.z < 0 ? 0 : readjusted_pos.z);

    const bool result = editor_widget_->handle_mouse_action(readjusted_pos, button_id, action_id, mouse_id);
    if (result) {
        // For some reason it can't regain focus when you mess around
        if (!displayer_->hasFocus()) {
            displayer_->setFocus();
        }
    }

    return result;
}

void main_window::on_mapping_editor_hidden() {
    if (!displayer_->hasFocus()) {
        displayer_->setFocus();
    }
}

bool main_window::deliver_key_event(const std::uint32_t code, const bool is_press) {
    if (is_press && editor_widget_->isVisible() && editor_widget_->handle_key_press(code)) {
        return true;
    }

    return map_executor_->execute(code, is_press);
}

void main_window::on_action_button_mapping_editor_triggered() {
    on_settings_triggered();
    settings_dialog_->set_active_tab(settings_dialog::CONTROL_TAB_INDEX);
}

void main_window::on_action_touch_mapping_editor_triggered() {
    editor_widget_->setVisible(true);
}

void main_window::on_exit_for_update_requested() {
    close();
}

void main_window::on_action_check_for_update_triggered() {
    update_dialog *diag = new update_dialog(this);
    connect(diag, &update_dialog::exit_for_update_request, this, &main_window::on_exit_for_update_requested);

    diag->check_for_update(true);
}

void main_window::load_and_show() {
    show();
    if (applist_) {
        applist_->request_reload();
    }
}

void main_window::on_action_launch_process_triggered() {
    launch_process_dialog *diag = new launch_process_dialog(this, emulator_state_.symsys->get_kernel_system());
    connect(diag, &launch_process_dialog::launched, this, &main_window::on_launch_process_requested);
    diag->show();
}

void main_window::on_launch_process_requested() {
    if (displayer_ && !displayer_->isVisible()) {
        switch_to_game_display_mode();
    }
}

void main_window::on_action_jar_triggered() {
    QSettings settings;
    QString last_install_dir;

    QVariant last_install_dir_variant = settings.value(LAST_JAR_FOLDER_SETTING);
    if (last_install_dir_variant.isValid()) {
        last_install_dir = last_install_dir_variant.toString();
    }

    QString package_file_path = QFileDialog::getOpenFileName(this, tr("Choose the JAR file to install"), last_install_dir, tr("JAR file (*.jar)"));
    if (!package_file_path.isEmpty()) {
        settings.setValue(LAST_JAR_FOLDER_SETTING, QFileInfo(package_file_path).absoluteDir().path());

        eka2l1::j2me::app_entry entry_res;
        const eka2l1::j2me::install_error err = eka2l1::j2me::install(emulator_state_.symsys.get(), package_file_path.toStdString(), entry_res);

        if (err == eka2l1::j2me::INSTALL_ERROR_JAR_SUCCESS) {
            QString info_str = tr("%1 version %2 (by %3) has been installed!").arg(QString::fromStdString(entry_res.title_),
                QString::fromStdString(entry_res.version_), QString::fromStdString(entry_res.author_));

            QMessageBox::information(this, tr("Install success"), info_str);
            
            eka2l1::j2me::app_list *list = emulator_state_.symsys->get_j2me_applist();
            list->flush();

            if (applist_) {
                applist_->request_reload(true);
            }
        } else {
            QString error_str;
            switch (err) {
            case eka2l1::j2me::INSTALL_ERROR_JAR_CANT_ADD_TO_DB:
                error_str = tr("Can not add the JAR to the apps database!");
                break;

            case eka2l1::j2me::INSTALL_ERROR_JAR_INVALID:
                error_str = tr("The given file is not a valid JAR file!");
                break;

            case eka2l1::j2me::INSTALL_ERROR_JAR_NOT_FOUND:
                error_str = tr("Can not find the JAR file!");
                break;

            case eka2l1::j2me::INSTALL_ERROR_JAR_ONLY_MIDP1_SUPPORTED:
                error_str = tr("The JAR needs MIDP-2.0, but the emulator only support MIDP-1.0 JAR running on S60v1 devices!");
                break;

            case eka2l1::j2me::INSTALL_ERROR_NOT_SUPPORTED_FOR_PLAT:
                error_str = tr("The JAR can not be installed for this current device! Only S60v1 devices can install this JAR at the moment");
                break;

            default:
                error_str = tr("An unexpected error has happened. Error code: %1").arg(static_cast<int>(err));
                break;
            }

            QMessageBox::critical(this, tr("Install failed"), error_str);
        }
    }
}

void main_window::on_btnet_friends_dialog_requested_from_conf() {
    spawn_btnet_friends_dialog(true);
}

void main_window::on_action_netplay_configure_triggered() {
    btnet_dialog_ = new btnet_dialog(this, emulator_state_.conf);
    connect(btnet_dialog_, &btnet_dialog::direct_ip_editor_requested, this, &main_window::on_btnet_friends_dialog_requested_from_conf);

    btnet_dialog_->show();
}

int main_window::map_mouse_id_to_touch_index(int mouse_id, const bool on_release) {
    if (!map_executor_->is_active()) {
        if (!mouse_to_touch_index_emu_.empty()) {
            for (auto ite: mouse_to_touch_index_emu_) {
                map_executor_->free_allocated_pointer(static_cast<std::uint8_t>(ite.second));
            }
            mouse_to_touch_index_emu_.clear();
        }
        return mouse_id;
    }
    auto ite = mouse_to_touch_index_emu_.find(mouse_id);
    if (ite != mouse_to_touch_index_emu_.end()) {
        if (on_release) {
            map_executor_->free_allocated_pointer(static_cast<std::uint8_t>(ite->second));
            mouse_to_touch_index_emu_.erase(mouse_id);
        }
        return ite->second;
    }
    if (on_release) {
        return 0;
    }
    int new_ptr_index = static_cast<int>(map_executor_->allocate_free_pointer());
    mouse_to_touch_index_emu_.emplace(mouse_id, new_ptr_index);

    return new_ptr_index;
}

void main_window::on_action_stretch_to_fill_toggled(bool checked) {
    emulator_state_.stretch_to_fill_display = checked;

    QSettings settings;
    settings.setValue(STRETCH_DISPLAY_SETTING, checked);
}

bool main_window::load_background_image(const std::string &path) {
    if (!eka2l1::common::exists(path)) {
        return false;
    }

    FILE *f = eka2l1::common::open_c_file(path, "rb");
    if (!f) {
        return false;
    }

    int x, y;
    int comp = STBI_rgb_alpha;

    stbi_uc *data = stbi_load_from_file(f, &x, &y, &comp, STBI_rgb_alpha);

    if (!data) {
        LOG_ERROR(eka2l1::FRONTEND_UI, "Unable to load background image!");
        return false;
    }

    if (!background_image_texture_) {
        background_image_texture_ = eka2l1::drivers::create_texture(emulator_state_.graphics_driver.get(), 2, 0,
            eka2l1::drivers::texture_format::rgba, eka2l1::drivers::texture_format::rgba,
            eka2l1::drivers::texture_data_type::ubyte, data, x * y * 4, eka2l1::vec3(x, y, 0));

        if (!background_image_texture_) {
            LOG_ERROR(eka2l1::FRONTEND_UI, "Unable to create background texture!");
            stbi_image_free(data);

            return false;
        }

        eka2l1::drivers::graphics_command_builder builder;
        builder.set_texture_filter(background_image_texture_, true, eka2l1::drivers::filter_option::nearest);
        
        auto cmd_list = builder.retrieve_command_list();
        emulator_state_.graphics_driver->submit_command_list(cmd_list);
    } else {
        eka2l1::drivers::graphics_command_builder builder;
        if (previous_background_image_size_ != eka2l1::vec2(x, y)) {
            builder.recreate_texture(background_image_texture_, 2, 0, eka2l1::drivers::texture_format::rgba,
                eka2l1::drivers::texture_format::rgba, eka2l1::drivers::texture_data_type::ubyte, data,
                x * y * 4, eka2l1::vec3(x, y, 0));
            builder.set_texture_filter(background_image_texture_, true, eka2l1::drivers::filter_option::nearest);
        } else {
            builder.update_texture(background_image_texture_, reinterpret_cast<const char*>(data), x * y * 4, 0, eka2l1::drivers::texture_format::rgba,
                eka2l1::drivers::texture_data_type::ubyte, eka2l1::vec3(0, 0, 0), eka2l1::vec3(x, y, 0));
        }
        auto cmd_list = builder.retrieve_command_list();
        emulator_state_.graphics_driver->submit_command_list(cmd_list);
    }

    previous_background_image_size_ = eka2l1::vec2(x, y);
    stbi_image_free(data);

    return true;
}

eka2l1::drivers::handle main_window::get_background_image() {
    if (emulator_state_.conf.background_image.empty()) {
        return 0;
    }

    if (!background_image_texture_ || (previous_background_image_path_ != emulator_state_.conf.background_image)) {
        load_background_image(emulator_state_.conf.background_image);

        // Force to prevent multiple reload attempt
        previous_background_image_path_ = emulator_state_.conf.background_image;
    }

    return background_image_texture_;
}

void main_window::on_question_dialog_open_request() {
    message_box_asyncable_close *msg_box = new message_box_asyncable_close(this, question_dialog_text_, question_dialog_button1_text_,
        question_dialog_button2_text_, question_dialog_complete_callback_);

    msg_box->show();
}

void main_window::question_dialog_open(const std::u16string &text, const std::u16string &button1_text, const std::u16string &button2_text,
    eka2l1::drivers::ui::yes_no_dialog_complete_callback complete_callback) {
    question_dialog_text_ = text;
    question_dialog_button1_text_ = button1_text;
    question_dialog_button2_text_ = button2_text;
    question_dialog_complete_callback_ = complete_callback;

    emit question_dialog_open_request();
}

void main_window::on_mouse_inactive() {
    setCursor(Qt::BlankCursor);
}

bool main_window::eventFilter(QObject *target, QEvent *event) {
    if (target == displayer_ && event->type() == QEvent::MouseMove) {
        setCursor(Qt::ArrowCursor);

        inactivity_timer_.stop();

        inactivity_timer_.setSingleShot(true);
        inactivity_timer_.start(MOUSE_INACTIVITY_TIMEOUT_MS);
    }

    return false;
}

void main_window::on_package_try_install_count_changed(const std::size_t count, const std::size_t total) {
    if (current_progress_dialog_) {
        total_try_installed_ = count;
        total_to_install_ = total;

        current_progress_dialog_->setLabelText(tr("Package %1/%2 - %3%").arg(count).arg(total)
            .arg(static_cast<float>(count) / static_cast<float>(total) * 100.0f, 0, 'f', 2));
    }
}

void main_window::on_icon_size_slider_value_changed(int value) {
    if (applist_) {
        applist_->set_icon_size(value);
    }
}