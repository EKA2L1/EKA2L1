#include <qt/applistwidget.h>
#include <qt/displaywidget.h>
#include <qt/mainwindow.h>
#include <qt/aboutdialog.h>
#include <qt/state.h>
#include "./ui_mainwindow.h"

#include <system/epoc.h>
#include <kernel/kernel.h>

#include <common/language.h>

#include <services/applist/applist.h>
#include <services/window/window.h>
#include <services/window/screen.h>
#include <services/fbs/fbs.h>
#include <package/manager.h>

#include <QFileDialog>
#include <QFuture>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
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

    cmd_builder->set_texture_filter(scr->screen_texture, filter, filter);
    cmd_builder->draw_bitmap(scr->screen_texture, 0, dest, src, eka2l1::vec2(0, 0), 0.0f, eka2l1::drivers::bitmap_draw_flag_no_flip);

    if (scr->dsa_texture) {
        cmd_builder->set_texture_filter(scr->dsa_texture, filter, filter);
        advance_dsa_pos_around_origin(dest, crr_mode.rotation);

        // Rotate back to original size
        if (crr_mode.rotation % 180 != 0) {
            std::swap(dest.size.x, dest.size.y);
            std::swap(src.size.x, src.size.y);
        }

        cmd_builder->draw_bitmap(scr->dsa_texture, 0, dest, src, eka2l1::vec2(0, 0), static_cast<float>(crr_mode.rotation), eka2l1::drivers::bitmap_draw_flag_no_flip);
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

    eka2l1::system *system = emulator_state_.symsys.get();
    if (system) {
        eka2l1::kernel_system *kernel = system->get_kernel_system();
        if (kernel) {
            const std::string al_server_name = eka2l1::get_app_list_server_name_by_epocver(kernel->get_epoc_version());
            const std::string fbs_server_name = eka2l1::epoc::get_fbs_server_name_by_epocver(kernel->get_epoc_version());

            eka2l1::applist_server *al_serv = reinterpret_cast<eka2l1::applist_server*>(kernel->get_by_name<eka2l1::service::server>(al_server_name));
            eka2l1::fbs_server *fbs_serv = reinterpret_cast<eka2l1::fbs_server*>(kernel->get_by_name<eka2l1::service::server>(fbs_server_name));

            if (al_serv && fbs_serv) {
                applist_ = new applist_widget(this, al_serv, fbs_serv);
                ui_->layout_main->addWidget(applist_);
            }
        }

        eka2l1::manager::packages *pkgmngr = system->get_packages();
        pkgmngr->show_text = [this](const char *text, const bool one_button) -> bool {
            // We only have one receiver, so it's ok ;)
            return emit package_install_text_ask(text, one_button);
        };

        pkgmngr->choose_lang = [this](const int *languages, const int language_count) -> int {
            return emit package_install_language_choose(languages, language_count);
        };
    }

    if (!applist_) {
        ui_->label_al_not_available->setVisible(true);
    }

    displayer_ = new display_widget(this);
    displayer_->setVisible(false);

    ui_->layout_main->addWidget(displayer_);

    connect(ui_->action_about, &QAction::triggered, this, &main_window::on_about_triggered);
    connect(ui_->action_package, &QAction::triggered, this, &main_window::on_package_install_clicked);

    connect(this, &main_window::package_install_progress_change, this, &main_window::on_package_install_progress_change);
    connect(this, &main_window::package_install_text_ask, this, &main_window::on_package_install_text_ask, Qt::BlockingQueuedConnection);
    connect(this, &main_window::package_install_language_choose, this, &main_window::on_package_install_language_choose, Qt::BlockingQueuedConnection);

    connect(applist_, &QListWidget::itemClicked, this, &main_window::on_app_clicked);
}

main_window::~main_window()
{
    if (applist_) {
        delete applist_;
    }

    delete ui_;
}

void main_window::on_about_triggered() {
    about_dialog *aboutDiag = new about_dialog(this);
    aboutDiag->show();
}

void main_window::setup_screen_draw() {
    eka2l1::system *system = emulator_state_.symsys.get();
    if (system) {
        eka2l1::kernel_system *kernel = system->get_kernel_system();
        if (kernel) {
            const std::string win_server_name = eka2l1::get_winserv_name_by_epocver(kernel->get_epoc_version());
            eka2l1::window_server *win_serv = reinterpret_cast<eka2l1::window_server*>(kernel->get_by_name<eka2l1::service::server>(win_server_name));

            if (win_serv) {
                eka2l1::epoc::screen *scr = win_serv->get_screens();
                while (scr) {
                    if (scr->number == active_screen_number_) {
                        active_screen_draw_callback_ = scr->add_screen_redraw_callback(&emulator_state_, draw_emulator_screen);
                        active_screen_mode_change_callback_ = scr->add_screen_mode_change_callback(&emulator_state_, mode_change_screen);

                        mode_change_screen(&emulator_state_, scr, 0);

                        break;
                    }

                    scr = scr->next;
                }
            }
        }
    }
}

void main_window::on_app_clicked(QListWidgetItem *item) {
    if (!active_screen_draw_callback_) {
        setup_screen_draw();
    }

    if (applist_->launch_from_widget_item(item)) {
        applist_->setVisible(false);
        displayer_->setVisible(true);
        displayer_->setFocus();
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

    QString result = QInputDialog::getItem(this, tr("Choose a language for the package"), QString(), language_string_list);
    if (result.isEmpty()) {
        return -1;
    }

    const int index = language_string_list.indexOf(result);
    if (index >= language_count) {
        LOG_ERROR(eka2l1::FRONTEND_UI, "The index of the choosen language is out of provided range! (index={})", index);
        return -1;
    }

    return languages[index];
}

void main_window::on_package_install_clicked() {
    QString package_file_path = QFileDialog::getOpenFileName(this, tr("Choose the file to install"),
                                                             QString(), tr("SIS files (*.sis *.sisx)"));

    if (!package_file_path.isEmpty()) {
        eka2l1::manager::packages *pkgmngr = emulator_state_.symsys->get_packages();
        if (pkgmngr) {
            current_package_install_dialog_ = new QProgressDialog(QString{}, tr("Cancel"), 0, 100, this);

            current_package_install_dialog_->setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::WindowMaximizeButtonHint);
            current_package_install_dialog_->setWindowTitle(tr("Installing package progress"));
            current_package_install_dialog_->show();

            QFuture<eka2l1::package::installation_result> install_future = QtConcurrent::run([this, pkgmngr, package_file_path]() {
                return pkgmngr->install_package(package_file_path.toStdU16String(), drive_e, [this](const std::size_t done, const std::size_t total) {
                    emit package_install_progress_change(done, total);
                });
            });

            while (!install_future.isFinished()) {
                QCoreApplication::processEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            current_package_install_dialog_->close();

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

void main_window::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);

    // Only care to redraw if displayer is active
    if (!displayer_->isVisible()) {
        return;
    }

    eka2l1::system *system = emulator_state_.symsys.get();
    if (system) {
        eka2l1::kernel_system *kernel = system->get_kernel_system();
        if (kernel) {
            const std::string win_server_name = eka2l1::get_winserv_name_by_epocver(kernel->get_epoc_version());
            eka2l1::window_server *win_serv = reinterpret_cast<eka2l1::window_server*>(kernel->get_by_name<eka2l1::service::server>(win_server_name));

            if (win_serv) {
                eka2l1::epoc::screen *scr = win_serv->get_screens();
                while (scr) {
                    if (scr->number == active_screen_number_) {
                        draw_emulator_screen(&emulator_state_, scr, true);
                        break;
                    }

                    scr = scr->next;
                }
            }
        }
    }
}
