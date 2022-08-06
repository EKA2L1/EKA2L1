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

#include <miniz.h>
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
        std::unique_ptr<mz_zip_archive> archive = std::make_unique<mz_zip_archive>();
        if (!mz_zip_reader_init_file(archive.get(), ZIP_STAGING_FILENAME, 0)) {
            return notify_error(tr("Downloaded update is not a zip file!"));
        }

        // Locate the system folder, if does not exist, is not a valid game card dump
        const std::uint32_t num_files = mz_zip_reader_get_num_files(archive.get());

        std::vector<std::pair<std::string, std::uint32_t>> list_files;

        for (std::uint32_t i = 0; i < num_files; i++) {
            mz_zip_archive_file_stat file_stat;
            if (mz_zip_reader_file_stat(archive.get(), i, &file_stat)) {
                if ((file_stat.m_external_attr & 0x10) == 0) {
                    std::string filename = file_stat.m_filename;
                    filename = filename.substr(0, common::min(strlen(UPDATER_FOLDER_NAME), filename.length()));

                    if (common::compare_ignore_case(filename.c_str(), UPDATER_FOLDER_NAME) == 0) {
                        continue;
                    }
                    total_uncomp_size_ += file_stat.m_uncomp_size;
                    list_files.push_back(std::make_pair(file_stat.m_filename, i));
                }
            } else {
                mz_zip_reader_end(archive.get());
                return notify_error(tr("The downloaded archive zip is corrupted"));
            }
        }

        std::string current_dir;
        common::get_current_directory(current_dir);

        const std::string temp_folder = eka2l1::absolute_path("temp\\", current_dir);

        eka2l1::common::delete_folder(temp_folder);
        eka2l1::common::create_directories(temp_folder);

        struct callback_data {
            FILE *stream_ = nullptr;
            update_window *self_;
            std::uint64_t uncomp_progress_ = 0;

            void reset() {
                if (stream_)
                    fclose(stream_);
            }

            ~callback_data() {
                reset();
            }
        } cb_data;

        cb_data.self_ = this;

        for (std::size_t extracted = 0; extracted < list_files.size(); extracted++) {
            const std::string path_to_file = eka2l1::add_path(temp_folder, list_files[extracted].first);
            common::create_directories(eka2l1::file_directory(path_to_file));

            cb_data.reset();
            cb_data.stream_ = common::open_c_file(path_to_file, "wb");

            if (!mz_zip_reader_extract_to_callback(
                    archive.get(), static_cast<mz_uint>(list_files[extracted].second), [](void *userdata, mz_uint64 offset, const void *buf, std::size_t n) -> std::size_t {
                        callback_data *data_ptr = reinterpret_cast<callback_data *>(userdata);
                        std::size_t written = fwrite(buf, 1, n, data_ptr->stream_);

                        if (written == n) {
                            data_ptr->uncomp_progress_ += written;
                            data_ptr->self_->emit_update_progress_bar(data_ptr->uncomp_progress_);
                        }

                        return static_cast<std::size_t>(written);
                    },
                    &cb_data, 0)) {
                cb_data.reset();
                eka2l1::common::delete_folder(temp_folder);

                mz_zip_reader_end(archive.get());
                return notify_error(tr("The downloaded archive zip is corrupted"));
            }

            emit_update_log(tr("Extracted: %1").arg(QString::fromStdString(list_files[extracted].first)));
        }

        mz_zip_reader_end(archive.get());

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
