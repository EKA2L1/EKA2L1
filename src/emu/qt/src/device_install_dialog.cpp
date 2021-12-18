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

#include "ui_device_install_dialog.h"
#include <qt/device_install_dialog.h>

#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>

#include <system/installation/firmware.h>
#include <system/installation/rpkg.h>

#include <config/config.h>
#include <system/devices.h>

#include <thread>

#include <QFileDialog>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>

#include <QInputDialog>
#include <QMessageBox>

static constexpr std::int32_t INSTALL_METHOD_DEVICE_DUMP_INDEX = 0;
static constexpr std::int32_t INSTALL_METHOD_FIRMWARE_INDEX = 1;

device_install_dialog::device_install_dialog(QWidget *parent, eka2l1::device_manager *dvcmngr, eka2l1::config::state &conf)
    : QDialog(parent)
    , conf_(conf)
    , device_mngr_(dvcmngr)
    , canceled_(false)
    , ui(new Ui::device_install_dialog) {
    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->setupUi(this);

    ui->confirmation_install_btn->setDisabled(true);
    ui->install_progress_bar->setVisible(false);

    on_current_index_changed(ui->installation_combo->currentIndex());

    connect(ui->installation_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &device_install_dialog::on_current_index_changed);
    connect(ui->vpl_browse_btn, &QPushButton::clicked, this, &device_install_dialog::on_vpl_browse_triggered);
    connect(ui->rom_browse_btn, &QPushButton::clicked, this, &device_install_dialog::on_rom_browse_triggered);
    connect(ui->rpkg_browse_btn, &QPushButton::clicked, this, &device_install_dialog::on_rpkg_browse_triggered);
    connect(ui->confirmation_install_btn, &QPushButton::clicked, this, &device_install_dialog::on_install_triggered);
    connect(ui->confirmation_cancel_btn, &QPushButton::clicked, this, &device_install_dialog::on_cancel_triggered);
    connect(this, &device_install_dialog::progress_bar_update, this, &device_install_dialog::on_progress_bar_update, Qt::QueuedConnection);
    connect(this, &device_install_dialog::firmware_variant_selects, this, &device_install_dialog::on_firmware_variant_selects, Qt::BlockingQueuedConnection);
}

device_install_dialog::~device_install_dialog() {
    delete ui;
}

void device_install_dialog::on_cancel_triggered() {
    canceled_ = true;

    if (!ui->install_progress_bar->isVisible()) {
        close();
    }
}

void device_install_dialog::on_current_index_changed(int new_index) {
    if (new_index < 0) {
        new_index = INSTALL_METHOD_DEVICE_DUMP_INDEX;
    }

    switch (new_index) {
    case INSTALL_METHOD_DEVICE_DUMP_INDEX:
        ui->vpl_browse_widget->setVisible(false);
        ui->rpkg_browse_widget->setVisible(false);
        ui->rom_browse_widget->setVisible(true);
        break;

    case INSTALL_METHOD_FIRMWARE_INDEX:
        ui->vpl_browse_widget->setVisible(true);
        ui->rpkg_browse_widget->setVisible(false);
        ui->rom_browse_widget->setVisible(false);
        break;

    default:
        break;
    }
}

void device_install_dialog::on_progress_bar_update(const std::size_t so_far, const std::size_t total) {
    ui->install_progress_bar->setValue(static_cast<int>(so_far * 100 / total));
}

int device_install_dialog::on_firmware_variant_selects(const std::vector<std::string> &list) {
    QStringList list_qstring;
    for (std::size_t i = 0; i < list.size(); i++) {
        list_qstring.push_back(QString::fromUtf8(list[i].c_str()));
    }

    bool go_fine = false;
    QString result = QInputDialog::getItem(this, tr("Choose the firmware variant to install"), QString(), list_qstring, 0, false, &go_fine);
    if (result.isEmpty() || !go_fine) {
        canceled_ = true;
        return -1;
    }

    const int index = list_qstring.indexOf(result);
    if (index >= list.size()) {
        LOG_ERROR(eka2l1::FRONTEND_UI, "The index of the choosen variant is out of provided range! (index={})", index);
        return -1;
    }

    return index;
}

void device_install_dialog::on_install_triggered() {
    ui->installation_choose_widget->setVisible(false);
    ui->install_progress_bar->setVisible(true);
    ui->confirmation_install_btn->setDisabled(true);

    QFuture<eka2l1::device_installation_error> install_future = QtConcurrent::run([this]() {
        const std::string root_c_path = eka2l1::add_path(conf_.storage, "drives/c/");
        const std::string root_e_path = eka2l1::add_path(conf_.storage, "drives/e/");
        const std::string root_z_path = eka2l1::add_path(conf_.storage, "drives/z/");
        const std::string rom_resident_path = eka2l1::add_path(conf_.storage, "roms/");

        eka2l1::common::create_directories(rom_resident_path);
        eka2l1::device_installation_error error = eka2l1::device_installation_none;
        bool need_copy_rom = false;

        auto progress_update_cb_func = [this](const std::size_t taken, const std::size_t total) {
            emit progress_bar_update(taken, total);
        };

        auto cancel_cb_func = [this]() {
            return canceled_.load();
        };

        auto select_variant_cb_func = [this](const std::vector<std::string> &list) {
            return emit firmware_variant_selects(list);
        };

        std::string firmware_code;

        if (ui->rom_browse_widget->isVisible()) {
            if (ui->rpkg_browse_widget->isVisible()) {
                error = eka2l1::loader::install_rpkg(device_mngr_, ui->rpkg_path_line_edit->text().toStdString(), root_z_path, firmware_code, progress_update_cb_func, cancel_cb_func);
                need_copy_rom = true;
            } else {
                error = eka2l1::loader::install_rom(device_mngr_, ui->rom_path_line_edit->text().toStdString(), rom_resident_path, root_z_path, progress_update_cb_func, cancel_cb_func);
            }
        }

        if (ui->vpl_browse_widget->isVisible()) {
            error = eka2l1::install_firmware(device_mngr_, ui->vpl_path_line_edit->text().toStdString(), root_c_path, root_e_path, root_z_path, rom_resident_path, select_variant_cb_func, progress_update_cb_func, cancel_cb_func);
        }

        if (error != eka2l1::device_installation_none) {
            return error;
        }

        device_mngr_->save_devices();

        if (need_copy_rom) {
            const std::string rom_directory = eka2l1::add_path(conf_.storage, eka2l1::add_path("roms", firmware_code + "\\"));
            eka2l1::common::create_directories(rom_directory);
            eka2l1::common::copy_file(ui->rom_path_line_edit->text().toStdString(), eka2l1::add_path(rom_directory, "SYM.ROM"), true);
        }

        return error;
    });

    while (!install_future.isFinished()) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (!canceled_) {
        QWidget *leecher = parentWidget();

        if (install_future.result() == eka2l1::device_installation_none) {
            eka2l1::device *lastest = device_mngr_->lastest();
            QMessageBox::information(leecher, tr("Installation completed"),
                tr("Device %1 (%2) has been successfully installed!").arg(QString::fromStdString(lastest->model), QString::fromStdString(lastest->firmware_code)));

            emit new_device_added();
        } else {
            QString error_string;

            switch (install_future.result()) {
            case eka2l1::device_installation_already_exist:
                error_string = tr("The device has already been installed!");
                break;

            case eka2l1::device_installation_determine_product_failure:
                error_string = tr("Fail to determine product information from the dump!");
                break;

            case eka2l1::device_installation_fpsx_corrupt:
                error_string = tr("One of the FPSX files provided in the firmware is corrupted!");
                break;

            case eka2l1::device_installation_general_failure:
                error_string = tr("An unknown error has occured. Please contact the developers with your log!");
                break;

            case eka2l1::device_installation_insufficent:
                error_string = tr("The disk space is insufficient to perform the device install! Please free your disk space!");
                break;

            case eka2l1::device_installation_not_exist:
                error_string = tr("Some files provided for installation do not exist anymore. Please keep them intact until the installation is done!");
                break;

            case eka2l1::device_installation_rofs_corrupt:
                error_string = tr("The ROFS in the firmware file is corrupted! Please make sure your firmware files are not corrupted.");
                break;

            case eka2l1::device_installation_rom_fail_to_copy:
                error_string = tr("Fail to copy ROM file!");
                break;

            case eka2l1::device_installation_rom_file_corrupt:
                error_string = tr("The provided ROM is corrupted! Please make sure your ROM is valid!");
                break;

            case eka2l1::device_installation_rpkg_corrupt:
                error_string = tr("The provided RPKG is corrupted! Please make sure your RPKG is valid!");
                break;

            case eka2l1::device_installation_vpl_file_invalid:
                error_string = tr("The provided VPL file is invalid. Please check your firmware files again!");
                break;

            default:
                break;
            }

            if (!error_string.isEmpty()) {
                QMessageBox::critical(leecher, tr("Installation failed"), error_string);
            }
        }
    }

    close();
}

void device_install_dialog::on_vpl_browse_triggered() {
    QString vpl_file_path = QFileDialog::getOpenFileName(this, tr("Choose VPL file"),
        QString(), tr("VPL file (*.vpl);;All files (*.*)"));

    if (!vpl_file_path.isEmpty()) {
        ui->vpl_path_line_edit->setText(vpl_file_path);
        ui->confirmation_install_btn->setDisabled(false);
    }
}

void device_install_dialog::on_rom_browse_triggered() {
    QString rom_file_path = QFileDialog::getOpenFileName(this, tr("Choose the ROM"),
        QString(), tr("ROM file (*.rom *.ROM);;All files (*.*)"));

    if (!rom_file_path.isEmpty()) {
        ui->rom_path_line_edit->setText(rom_file_path);
        const std::string rom_file_path_std = rom_file_path.toStdString();

        if (eka2l1::common::exists(rom_file_path_std)) {
            if (eka2l1::loader::should_install_requires_additional_rpkg(rom_file_path_std)) {
                ui->rpkg_browse_widget->setVisible(true);

                if (!ui->rpkg_path_line_edit->text().isEmpty()) {
                    ui->confirmation_install_btn->setDisabled(false);
                } else {
                    ui->confirmation_install_btn->setDisabled(true);
                }
            } else {
                ui->rpkg_browse_widget->setVisible(false);
                ui->confirmation_install_btn->setDisabled(false);
            }
        }
    }
}

void device_install_dialog::on_rpkg_browse_triggered() {
    QString rpkg_file_path = QFileDialog::getOpenFileName(this, tr("Choose the RPKG"),
        QString(), tr("RPKG file (*.rpkg *.RPKG);;All files (*.*"));

    if (!rpkg_file_path.isEmpty()) {
        ui->rpkg_path_line_edit->setText(rpkg_file_path);
        ui->confirmation_install_btn->setDisabled(false);
    }
}
