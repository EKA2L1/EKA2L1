/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include <QMainWindow>
#include <QMessageBox>
#include <QDir>

#include <QFuture>
#include <QtConcurrent/QtConcurrent>

#include "ui_updater.h"

#include <Windows.h>

#include <common/algorithm.h>
#include <common/pystr.h>
#include <common/fileutils.h>
#include <common/path.h>

#include <common/archive.h>
#include <memory>

#include <cstdio>

using namespace eka2l1;

static const char *ZIP_STAGING_FILENAME = "..\\staging\\update.zip";
static const char *UPDATER_FOLDER_NAME = "updater";

class update_window : public QMainWindow {
    Q_OBJECT

private:
    Ui::update_window *ui_;
    std::uint64_t total_uncomp_size_;

signals:
    void update_progress_bar(const std::uint64_t pr, const bool copy_stage);
    void update_log_add(const QString &log);

public:
    explicit update_window(QWidget *parent = nullptr)
        : QMainWindow(parent)
        , ui_(new Ui::update_window)
        , total_uncomp_size_(0) {
        ui_->setupUi(this);

        connect(this, &update_window::update_progress_bar, this, &update_window::on_update_progress_bar_request, Qt::QueuedConnection);
        connect(this, &update_window::update_log_add, this, &update_window::on_update_log_add_request, Qt::QueuedConnection);
    }

    ~update_window() {
        delete ui_;
    }

    void emit_update_progress_bar(const std::uint64_t pr, const bool copy_stage = false) {
        emit update_progress_bar(pr, copy_stage);
    }

    void on_update_progress_bar_request(const std::uint64_t pr, const bool copy_stage) {
        if (copy_stage) {
            ui_->extracted_progress->setValue(100 + pr);
        } else {
            ui_->extracted_progress->setValue(pr * 100 / total_uncomp_size_);
        }
    }

    void emit_update_log(const QString &log) {
        emit update_log_add(log);
    }

    void on_update_log_add_request(const QString &log) {
        ui_->extracted_log->append(log + "\n");
    }

    bool notify_error(const QString &err) {
        QMessageBox::critical(this, tr("Update failed"), err);
        common::delete_folder("temp\\");
        return false;
    }

    bool extract() {
        std::vector<common::archive_entry_info> entries;

        if (!common::list_archive(ZIP_STAGING_FILENAME, entries)) {
            return notify_error(tr("Downloaded update is not a zip file!"));
        }

        // The updater is running out of its own folder, so its files are the one thing the update must
        // not overwrite.
        const auto wanted = [](const common::archive_entry_info &entry) {
            if (entry.is_directory) {
                return false;
            }

            const std::string head = entry.path.substr(0,
                common::min(strlen(UPDATER_FOLDER_NAME), entry.path.length()));

            return common::compare_ignore_case(head.c_str(), UPDATER_FOLDER_NAME) != 0;
        };

        // Progress runs against the whole archive, skipped entries included: that is the total the
        // extractor counts against, and it still has to read past what we do not want.
        for (const common::archive_entry_info &entry : entries) {
            total_uncomp_size_ += entry.size;
        }

        std::string current_dir;
        common::get_current_directory(current_dir);

        const std::string temp_folder = eka2l1::absolute_path("temp\\", current_dir);

        eka2l1::common::delete_folder(temp_folder);
        eka2l1::common::create_directories(temp_folder);

        const bool unpacked = common::extract_archive(ZIP_STAGING_FILENAME,
            [&](const common::archive_entry_info &entry) -> std::string {
                if (!wanted(entry)) {
                    return std::string();
                }

                emit_update_log(tr("Extracted: %1").arg(QString::fromStdString(entry.path)));
                return eka2l1::add_path(temp_folder, entry.path);
            },
            [this](const std::size_t done, const std::size_t total) {
                if (total) {
                    emit_update_progress_bar(done);
                }
            },
            nullptr);

        if (!unpacked) {
            eka2l1::common::delete_folder(temp_folder);
            return notify_error(tr("The downloaded archive zip is corrupted"));
        }

        common::copy_folder(temp_folder, "..", 0, [this](const std::uint64_t current, const std::uint64_t total) {
            emit_update_progress_bar(100 * current / total, true);
        });

        emit_update_progress_bar(100, true);

        common::delete_folder(temp_folder);
        common::remove(ZIP_STAGING_FILENAME);

        return true;
    }

    void run() {
        if (!common::exists(ZIP_STAGING_FILENAME)) {
            notify_error(tr("Update's archive file does not exist!"));
            close();
            return;
        }

        QFuture<bool> extract_future = QtConcurrent::run([this]() -> bool {
            return extract();
        });

        while (!extract_future.isFinished()) {
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (extract_future.result()) {
            if (QMessageBox::question(this, tr("Update success"), tr("Update success. Relaunch the emulator?")) == QMessageBox::StandardButton::Yes) {
                QDir current;
                current.cdUp();
                
                QProcess *emu_process = new QProcess();
                emu_process->setProgram("..\\eka2l1_qt.exe");
                emu_process->setWorkingDirectory(current.absolutePath());
                emu_process->startDetached();
            }
        }

        close();
    }
};

#include "updater.moc"

static void wait_for_emulator_to_close(const std::uint32_t pid) {
    HANDLE process_handle = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!process_handle)
      return;

    WaitForSingleObject(process_handle, INFINITE);
    CloseHandle(process_handle);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
    common::pystr command_line(lpCmdLine);
    std::vector<common::pystr> args = command_line.split(" ");
    char **arg_cstr = new char*[args.size()];

    for (std::size_t i = 0; i < args.size(); i++) {
        arg_cstr[i] = args[i].cstr_mod();
    }

    int argc = static_cast<int>(args.size());
    QApplication a(argc, arg_cstr);

    if (argc >= 1) {
        std::int64_t pid_or_false = args[0].as_int<std::int64_t>(-1);
        if (pid_or_false >= 0) {
            wait_for_emulator_to_close(static_cast<std::uint32_t>(pid_or_false));
        }
    }

    QCoreApplication::setOrganizationName("EKA2L1");
    QCoreApplication::setApplicationName("Updater");

    update_window *update_win = new update_window();
    update_win->show();
    update_win->run();

    delete[] arg_cstr;
    return 0;
}
