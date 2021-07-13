#include <qt/applistwidget.h>
#include <qt/displaywidget.h>
#include <qt/mainwindow.h>
#include <qt/aboutdialog.h>
#include <qt/device_install_dialog.h>
#include <qt/settings_dialog.h>
#include <qt/state.h>
#include <qt/utils.h>
#include "./ui_mainwindow.h"

#include <system/epoc.h>
#include <system/devices.h>
#include <kernel/kernel.h>

#include <common/algorithm.h>
#include <common/fileutils.h>
#include <common/language.h>
#include <common/platform.h>
#include <common/path.h>

#include <config/app_settings.h>

#include <services/applist/applist.h>
#include <services/ui/cap/oom_app.h>
#include <services/window/window.h>
#include <services/window/screen.h>
#include <services/fbs/fbs.h>
#include <package/manager.h>

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFuture>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QtConcurrent/QtConcurrent>

static void advance_dsa_pos_around_origin(eka2l1::rect &origin_normal_rect, const int rotation) {
    switch (rotation) {
    case 90:
        origin_normal_rect.top.x += origin_normal_rect.size.x;
        break;

    case 180:
        origin_normal_rect.top.x += origin_normal_rect.size.x;
        break;

    case 270:
        origin_normal_rect.top.y += origin_normal_rect.size.y;
        break;

    default:
        break;
    }
}

static void mode_change_screen(void *userdata, eka2l1::epoc::screen *scr, const int old_mode) {
    eka2l1::desktop::emulator *state_ptr = reinterpret_cast<eka2l1::desktop::emulator*>(userdata);
    if (!state_ptr) {
        return;
    }

    display_widget *widget = static_cast<display_widget*>(state_ptr->window);
    widget->setMinimumSize(QSize(scr->current_mode().size.x, scr->current_mode().size.y));
}

static void draw_emulator_screen(void *userdata, eka2l1::epoc::screen *scr, const bool is_dsa) {
    eka2l1::desktop::emulator *state_ptr = reinterpret_cast<eka2l1::desktop::emulator*>(userdata);
    if (!state_ptr) {
        return;
    }

    eka2l1::desktop::emulator &state = *state_ptr;

    std::unique_ptr<eka2l1::drivers::graphics_command_list> cmd_list = state.graphics_driver->new_command_list();
    std::unique_ptr<eka2l1::drivers::graphics_command_list_builder> cmd_builder = state.graphics_driver->new_command_builder(
            cmd_list.get());

    eka2l1::rect viewport;
    eka2l1::rect src;
    eka2l1::rect dest;

    eka2l1::drivers::filter_option filter = state.conf.nearest_neighbor_filtering ? eka2l1::drivers::filter_option::nearest :
        eka2l1::drivers::filter_option::linear;

    const auto window_width = state.window->window_fb_size().x;
    const auto window_height = state.window->window_fb_size().y;

    eka2l1::vec2 swapchain_size(window_width, window_height);
    viewport.size = swapchain_size;
    cmd_builder->set_swapchain_size(swapchain_size);

    cmd_builder->backup_state();

    cmd_builder->clear({ 0xFF, 0xD0, 0xD0, 0xD0 }, eka2l1::drivers::draw_buffer_bit_color_buffer);
    cmd_builder->set_cull_mode(false);
    cmd_builder->set_depth(false);
    cmd_builder->set_viewport(viewport);

    auto &crr_mode = scr->current_mode();

    eka2l1::vec2 size = crr_mode.size;
    src.size = size;

    float mult = (float)(window_width) / size.x;
    float width = size.x * mult;
    float height = size.y * mult;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    if (height > swapchain_size.y) {
        height = swapchain_size.y;
        mult = height / size.y;
        width = size.x * mult;
        x = (swapchain_size.x - width) / 2;
    }
    scr->scale_x = mult;
    scr->scale_y = mult;
    scr->absolute_pos.x = static_cast<int>(x);
    scr->absolute_pos.y = static_cast<int>(y);

    dest.top = eka2l1::vec2(x, y);
    dest.size = eka2l1::vec2(width, height);

    if (scr->last_texture_access) {
        cmd_builder->set_texture_filter(scr->dsa_texture, filter, filter);
        advance_dsa_pos_around_origin(dest, crr_mode.rotation);

        // Rotate back to original size
        if (crr_mode.rotation % 180 != 0) {
            std::swap(dest.size.x, dest.size.y);
            std::swap(src.size.x, src.size.y);
        }

        cmd_builder->draw_bitmap(scr->dsa_texture, 0, dest, src, eka2l1::vec2(0, 0), static_cast<float>(crr_mode.rotation), eka2l1::drivers::bitmap_draw_flag_no_flip);
    } else {
        cmd_builder->set_texture_filter(scr->screen_texture, filter, filter);
        cmd_builder->draw_bitmap(scr->screen_texture, 0, dest, src, eka2l1::vec2(0, 0), 0.0f, eka2l1::drivers::bitmap_draw_flag_no_flip);
    }

    cmd_builder->load_backup_state();

    int wait_status = -100;

    // Submit, present, and wait for the presenting
    cmd_builder->present(&wait_status);

    state.graphics_driver->submit_command_list(*cmd_list);
    state.graphics_driver->wait_for(&wait_status);
}

main_window::main_window(QWidget *parent, eka2l1::desktop::emulator &emulator_state)
    : QMainWindow(parent)
    , emulator_state_(emulator_state)
    , active_screen_number_(0)
    , active_screen_draw_callback_(0)
    , active_screen_mode_change_callback_(0)
    , ui_(new Ui::main_window)
    , applist_(nullptr)
    , displayer_(nullptr)
{
    ui_->setupUi(this);
    ui_->label_al_not_available->setVisible(false);

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

    QSettings settings;
    ui_->status_bar->setVisible(!settings.value(STATUS_BAR_HIDDEN_SETTING_NAME, false).toBool());
    active_screen_number_ = settings.value(SHOW_SCREEN_NUMBER_SETTINGS_NAME, 0).toInt();

    connect(ui_->action_about, &QAction::triggered, this, &main_window::on_about_triggered);
    connect(ui_->action_settings, &QAction::triggered, this, &main_window::on_settings_triggered);
    connect(ui_->action_package, &QAction::triggered, this, &main_window::on_package_install_clicked);
    connect(ui_->action_device, &QAction::triggered, this, &main_window::on_device_install_clicked);
    connect(ui_->action_mount_game_card_dump, &QAction::triggered, this, &main_window::on_mount_card_clicked);
    connect(ui_->action_fullscreen, &QAction::toggled, this, &main_window::on_fullscreen_toogled);
    connect(ui_->action_restart, &QAction::triggered, this, &main_window::on_restart_requested);

    connect(this, &main_window::package_install_progress_change, this, &main_window::on_package_install_progress_change);
    connect(this, &main_window::status_bar_update, this, &main_window::on_status_bar_update);
    connect(this, &main_window::package_install_text_ask, this, &main_window::on_package_install_text_ask, Qt::BlockingQueuedConnection);
    connect(this, &main_window::package_install_language_choose, this, &main_window::on_package_install_language_choose, Qt::BlockingQueuedConnection);

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

        eka2l1::applist_server *al_serv = reinterpret_cast<eka2l1::applist_server*>(kernel->get_by_name<eka2l1::service::server>(al_server_name));
        eka2l1::fbs_server *fbs_serv = reinterpret_cast<eka2l1::fbs_server*>(kernel->get_by_name<eka2l1::service::server>(fbs_server_name));

        if (al_serv && fbs_serv) {
            applist_ = new applist_widget(this, al_serv, fbs_serv, system->get_io_system());
            ui_->layout_main->addWidget(applist_);

            connect(applist_, &applist_widget::app_launch, this, &main_window::on_app_clicked);
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

    delete recent_mount_folder_menu_;
    delete recent_mount_clear_;

    for (std::size_t i = 0; i < MAX_RECENT_ENTRIES; i++) {
        delete recent_mount_folder_actions_[i];
    }

    delete current_device_label_;
    delete screen_status_label_;

    delete ui_;
}

void main_window::refresh_current_device_label() {
    eka2l1::device_manager *dvcmngr = emulator_state_.symsys->get_device_manager();
    if (dvcmngr) {
        eka2l1::device *crr = dvcmngr->get_current();
        if (crr) {
            current_device_label_->setText(QString("%1 (%2)").arg(QString::fromUtf8(crr->model.c_str()), QString::fromUtf8(crr->firmware_code)));
        }
    }
}

void main_window::on_about_triggered() {
    about_dialog *about_diag = new about_dialog(this);
    about_diag->show();
}

void main_window::on_settings_triggered() {
    settings_dialog *settings_diag = new settings_dialog(this, emulator_state_.symsys.get(), emulator_state_.joystick_controller.get(),
                                                         emulator_state_.app_settings.get(), emulator_state_.conf);

    connect(settings_diag, &settings_dialog::cursor_visibility_change, this, &main_window::on_cursor_visibility_change);
    connect(settings_diag, &settings_dialog::status_bar_visibility_change, this, &main_window::on_status_bar_visibility_change);
    connect(settings_diag, &settings_dialog::relaunch, this, &main_window::on_relaunch_request);
    connect(settings_diag, &settings_dialog::restart, this, &main_window::on_device_set_requested);
    connect(settings_diag, &settings_dialog::active_app_setting_changed, this, &main_window::on_app_setting_changed, Qt::DirectConnection);
    connect(this, &main_window::app_launching, settings_diag, &settings_dialog::on_app_launching);
    connect(this, &main_window::controller_button_press, settings_diag, &settings_dialog::on_controller_button_press);
    connect(this, &main_window::screen_focus_group_changed, settings_diag, &settings_dialog::refresh_app_configuration_details);
    connect(this, &main_window::restart_requested, settings_diag, &settings_dialog::on_restart_requested_from_main);

    settings_diag->show();
}

void main_window::on_device_set_requested(const int index) {
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

    setup_app_list();
    refresh_current_device_label();
    screen_status_label_->clear();

    ui_->action_pause->setEnabled(false);
    ui_->action_restart->setEnabled(false);
}

void main_window::on_restart_requested() {
    on_device_set_requested(-1);
    emit restart_requested();
}

void main_window::on_relaunch_request() {
    QString program = QApplication::applicationFilePath();
    QStringList arguments = QApplication::arguments();
    QString workingDirectory = QDir::currentPath();
    QProcess::startDetached(program, arguments, workingDirectory);

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
        ui_->status_bar->hide();
        ui_->menu_bar->setVisible(false);

        showFullScreen();
    } else {
        ui_->status_bar->show();
        ui_->menu_bar->setVisible(true);

        showNormal();
    }

    displayer_->set_fullscreen(checked);
}

void main_window::refresh_recent_mounts() {
    QSettings settings;
    QStringList recent_mount_folders = settings.value(RECENT_MOUNT_SETTINGS_NAME).toStringList();

    std::size_t i = 0;

    for (; i < eka2l1::common::min(recent_mount_folders.size(), static_cast<qsizetype>(MAX_RECENT_ENTRIES)); i++) {
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

void main_window::mount_game_card_dump(QString mount_folder) {
    if (eka2l1::is_separator(mount_folder.back().unicode())) {
        // We don't care much about the last separator. This is for system folder check ;)
        mount_folder.erase(mount_folder.begin() + mount_folder.size() - 1, mount_folder.end());
    }

    if (mount_folder.endsWith("system", Qt::CaseInsensitive)) {
#if !EKA2L1_PLATFORM(WIN32)
        if (mount_folder.endsWith("system", Qt::CaseSensitive)) {
            QMessageBox::Information(this, tr("The game card dump has case-sensitive files. This may cause problems with the emulator."));
        }
#endif

        QMessageBox::StandardButton result = QMessageBox::question(this, tr("Game card dump folder correction"), tr("The selected path seems to be incorrect.<br>"
            "Do you want the emulator to correct it?"));

        if (result == QMessageBox::Yes) {
            mount_folder.erase(mount_folder.begin() + mount_folder.length() - 7, mount_folder.end());
        }
    }

    eka2l1::io_system *io = emulator_state_.symsys->get_io_system();

    io->unmount(drive_e);
    io->mount_physical_path(drive_e, drive_media::physical, io_attrib_removeable | io_attrib_write_protected, mount_folder.toStdU16String());

    QSettings settings;
    QStringList recent_mount_folders = settings.value(RECENT_MOUNT_SETTINGS_NAME).toStringList();

    if (recent_mount_folders.size() == MAX_RECENT_ENTRIES) {
        recent_mount_folders.removeLast();
    }

    if (recent_mount_folders.indexOf(mount_folder) == -1)
        recent_mount_folders.prepend(mount_folder);

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
    QAction *being = qobject_cast<QAction*>(sender());
    if (being) {
        mount_game_card_dump(being->data().toString());
    }
}

void main_window::on_mount_card_clicked() {
    QString mount_folder = QFileDialog::getExistingDirectory(this, tr("Choose the game card dump folder"));
    if (!mount_folder.isEmpty()) {
        mount_game_card_dump(mount_folder);
    }
}

void main_window::setup_screen_draw() {
    eka2l1::system *system = emulator_state_.symsys.get();
    if (system) {
        eka2l1::epoc::screen *scr = get_current_active_screen();
        if (scr) {
            active_screen_draw_callback_ = scr->add_screen_redraw_callback(&emulator_state_, [this](void *userdata, eka2l1::epoc::screen *scr, const bool is_dsa) {
                draw_emulator_screen(userdata, scr, is_dsa);
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

void main_window::on_app_clicked(applist_widget_item *item) {
    if (!active_screen_draw_callback_) {
        setup_screen_draw();
    }

    if (applist_->launch_from_widget_item(item)) {
        applist_->setVisible(false);
        displayer_->setVisible(true);
        displayer_->setFocus();

        ui_->action_pause->setEnabled(true);
        ui_->action_restart->setEnabled(true);

        on_fullscreen_toogled(ui_->action_fullscreen->isChecked());

        emit app_launching();
    } else {
        LOG_ERROR(eka2l1::FRONTEND_UI, "Fail to launch the selected application!");
    }
}

void main_window::on_package_install_progress_change(const std::size_t now, const std::size_t total) {
    current_package_install_dialog_->setValue(static_cast<int>(now * 100 / total));
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
            current_package_install_dialog_ = new QProgressDialog(QString{}, tr("Cancel"), 0, 100, this);

            current_package_install_dialog_->setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::WindowMaximizeButtonHint & ~Qt::WindowCloseButtonHint);
            current_package_install_dialog_->setWindowTitle(tr("Installing package progress"));
            current_package_install_dialog_->show();

            QFuture<eka2l1::package::installation_result> install_future = QtConcurrent::run([this, pkgmngr, package_file_path]() {
                return pkgmngr->install_package(package_file_path.toStdU16String(), drive_e, [this](const std::size_t done, const std::size_t total) {
                    emit package_install_progress_change(done, total);
                }, [this] {
                    return current_package_install_dialog_->wasCanceled();
                });
            });

            while (!install_future.isFinished()) {
                QCoreApplication::processEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            bool no_more_info = false;

            if (current_package_install_dialog_->wasCanceled()) {
                no_more_info = true;
            }

            current_package_install_dialog_->close();

            if (!no_more_info) {
                switch (install_future.result()) {
                case eka2l1::package::installation_result_aborted: {
                    QMessageBox::information(this, tr("Installation aborted"), tr("The installation has been canceled"));
                    break;
                }

                case eka2l1::package::installation_result_invalid: {
                    QMessageBox::critical(this, tr("Installation failed"), tr("Fail to install package at path: %1. "
                        "Ensure the path points to a valid SIS/SISX file.").arg(package_file_path));
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
        }
    }
}

void main_window::on_package_install_clicked() {
    QString package_file_path = QFileDialog::getOpenFileName(this, tr("Choose the file to install"),
                                                             QString(), tr("SIS file (*.sis *.sisx)"));
    spawn_package_install_camper(package_file_path);
}

void main_window::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);

    // Only care to redraw if displayer is active
    if (!displayer_->isVisible()) {
        return;
    }

    eka2l1::system *system = emulator_state_.symsys.get();
    if (system) {
        eka2l1::epoc::screen *scr = get_current_active_screen();
        if (scr) {
            draw_emulator_screen(&emulator_state_, scr, true);
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

bool main_window::controller_event_handler(eka2l1::drivers::input_event &event) {
    emit controller_button_press(event);
    return (displayer_ && displayer_->isVisible() && displayer_->hasFocus());
}

void main_window::make_default_binding_profile() {
    // Create default profile if there is none
    eka2l1::common::dir_iterator ite("bindings\\");
    eka2l1::common::dir_entry entry;

    int entry_count = 0;

    while (ite.next_entry(entry) >= 0) {
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

    eka2l1::config::app_setting setting;
    setting.fps = scr->refresh_rate;
    setting.time_delay = info->associated_->get_time_delay();
    setting.child_inherit_setting = info->associated_->get_child_inherit_setting();

    emulator_state_.app_settings->add_or_replace_setting(info->app_uid_, setting);
}
