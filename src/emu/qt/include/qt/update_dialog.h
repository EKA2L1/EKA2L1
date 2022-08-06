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

#pragma once

#include <QDialog>
#include <QString>

#include <string>

namespace Ui {
    class update_dialog;
}

class QNetworkAccessManager;
class QNetworkReply;
class QFile;

class update_dialog : public QDialog {
    Q_OBJECT

private:
    Ui::update_dialog *ui_;
    QNetworkAccessManager *access_manager_;

    std::string new_commit_hash_;
    QString download_url_;
    QNetworkReply *download_reply_;
    QFile *downloaded_file_;

    bool explicit_up_;

    void tag_download_link_request();
    void changelog_request();
    void report_error(const QString &error_string);
    void report_info(const QString &info_string);
    void revert_ui_to_update_wait();
    void update_encounter_error(const QString &error);

private slots:
    void on_tag_request_complete(QNetworkReply *reply);
    void on_tag_download_link_request_complete(QNetworkReply *reply);
    void on_changelog_request_complete(QNetworkReply *reply);
    void on_update_button_clicked();
    void on_ignore_button_clicked();
    void on_cancel_button_clicked();
    void on_new_update_download_progress_report(quint64 received, quint64 total);
    void on_new_update_download_read_ready();
    void on_new_update_download_finished(QNetworkReply *reply);
    void on_manual_check_checkbox_toggled(bool checked);

signals:
    void exit_for_update_request();

public:
    explicit update_dialog(QWidget *parent);
    ~update_dialog();

    void check_for_update(const bool explicit_up = false);
};
