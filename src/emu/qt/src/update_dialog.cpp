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

#include <qt/update_dialog.h>
#include "ui_update_dialog.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

#include <common/algorithm.h>
#include <common/configure.h>
#include <common/version.h>
#include <common/fileutils.h>
#include <common/platform.h>
#include <common/path.h>
#include <common/log.h>

#include <common/archive.h>

#include <cstring>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QSettings>

// Installing an update in place is Windows-only: it hands the downloaded zip to
// updater.exe, a separate program the build only produces for Windows, and that
// program overwrites an install made of loose files. The other desktops ship one
// AppImage and an ad-hoc signed .app inside a DMG, neither of which that
// mechanism can unpack over an existing install.
#if EKA2L1_PLATFORM(WIN32) && BUILD_FOR_USER
#define ENABLE_UPDATER 1
#else
#define ENABLE_UPDATER 0
#endif

// Checking, however, is three GitHub requests and nothing platform specific, so
// every desktop can tell the user a new build exists and hand them the download.
// Android is __unix__ too, and ships through its own store, so it is excluded.
#if (EKA2L1_PLATFORM(WIN32) || EKA2L1_PLATFORM(MACOS) || (EKA2L1_PLATFORM(UNIX) && !EKA2L1_PLATFORM(ANDROID))) && BUILD_FOR_USER
#define ENABLE_UPDATE_CHECK 1
#else
#define ENABLE_UPDATE_CHECK 0
#endif

// Code takes many reference from DuckStation!
static const char *TAG_RELEASE_INFO_REQUEST_GET_URL = "https://api.github.com/repos/eka2l1/eka2l1/releases/tags/%1";
static const char *COMMITS_COMPARE_REQUEST_GET_URL = "https://api.github.com/repos/eka2l1/eka2l1/compare/%1...%2";
static const char *UPDATE_TAG_NAME = "continous";

#if ENABLE_UPDATE_CHECK
static const char *TAG_REQUEST_GET_URL = "https://api.github.com/repos/eka2l1/eka2l1/tags";
#endif

#if ENABLE_UPDATER
static const char *UPDATE_FILE_STORE_FOLDER = "staging";
static const char *UPDATE_FILE_STORE_PATH = "staging\\update.zip";
#endif

static const char *INVALID_JSON_ERR_STR = "Invalid response from GitHub about newest update! Please check your internet connection!";
static const char *NO_AUTO_CHECK_UPDATE_SETTING = "noAutoCheckUpdate";

// These have to match the names the "roll-release" job of .github/workflows/build.yml
// uploads to the rolling release, or the updater will never find anything to download.
static QString get_platform_release_filename() {
#if EKA2L1_PLATFORM(WIN32)
    return QString("EKA2L1-Windows-x86_64.zip");
#elif EKA2L1_PLATFORM(UNIX)
    return QString("EKA2L1-Linux-x86_64.AppImage");
#else
    return QString("EKA2L1-MacOS-arm64.dmg");
#endif
}

update_dialog::update_dialog(QWidget *parent)
    : QDialog(parent)
    , ui_(new Ui::update_dialog)
    , access_manager_(new QNetworkAccessManager(this))
    , download_reply_(nullptr)
    , downloaded_file_(nullptr)
    , explicit_up_(false) {
    ui_->setupUi(this);
    ui_->cancel_button->hide();
    ui_->download_progress_bar->hide();

    QSettings settings;
    const bool dont_auto_update = settings.value(NO_AUTO_CHECK_UPDATE_SETTING, false).toBool();
    ui_->manual_check_checkbox->setChecked(dont_auto_update);

    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);

    setVisible(false);
}

update_dialog::~update_dialog() {
    if (downloaded_file_) {
        downloaded_file_->flush();
        downloaded_file_->close();

        delete downloaded_file_;
    }
    delete ui_;
    delete access_manager_;
}

// Both of these end the update check, so they also close the dialog: an
// automatic check keeps it hidden, and a hidden dialog that is never closed is
// never deleted either (WA_DeleteOnClose).
void update_dialog::report_info(const QString &info_string) {
    LOG_INFO(eka2l1::FRONTEND_UI, "Update check: {}", info_string.toStdString());

    if (explicit_up_) {
        QMessageBox::information(this, tr("Update success"), info_string);
    }

    close();
}

void update_dialog::report_error(const QString &info_string) {
    // An automatic check shows nothing, so the log is the only trace of why it
    // gave up.
    LOG_ERROR(eka2l1::FRONTEND_UI, "Update check failed: {}", info_string.toStdString());

    if (explicit_up_) {
        QMessageBox::critical(this, tr("Update failed"), info_string);
    }

    close();
}

void update_dialog::on_tag_request_complete(QNetworkReply *reply) {
    access_manager_->disconnect(this);
    reply->deleteLater();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray reply_data = reply->readAll();

        QJsonParseError parse_error;
        QJsonDocument doc = QJsonDocument::fromJson(reply_data, &parse_error);

        if (doc.isArray()) {
            bool found = false;

            QJsonArray doc_array = doc.array();
            for (const QJsonValue &tag_info: doc_array) {
                if (tag_info["name"].toString() == UPDATE_TAG_NAME) {
                    const std::string &current_sha = tag_info["commit"].toObject()["sha"].toString().toStdString();
                    const int commit_length = eka2l1::common::min<int>(static_cast<int>(current_sha.length()),
                                                                       static_cast<int>(strlen(GIT_COMMIT_HASH)));

                    if (current_sha.substr(0, commit_length) != (std::string(GIT_COMMIT_HASH).substr(0, commit_length))) {
                        // Hash different, need update
                        new_commit_hash_ = current_sha.substr(0, commit_length);
                        tag_download_link_request();
                    } else {
                        report_info(tr("The emulator is already updated to lastest version!"));
                    }

                    found = true;
                    break;
                }
            }

            if (!found) {
                report_error(tr("The release channel is not available. Please check your internet connection!"));
            }
        } else {
            report_error(tr(INVALID_JSON_ERR_STR));
        }
    } else {
        report_error(tr("Failed to get information about the newest update from GitHub!"));
    }
}

void update_dialog::on_tag_download_link_request_complete(QNetworkReply *reply) {
    access_manager_->disconnect(this);
    reply->deleteLater();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray reply_data = reply->readAll();

        QJsonParseError parse_error;
        QJsonDocument doc = QJsonDocument::fromJson(reply_data, &parse_error);

        if (doc.isObject()) {
            QJsonArray asset_array = doc["assets"].toArray();
            const QString platform_asset_name = get_platform_release_filename();

            release_page_url_ = doc["html_url"].toString();

            for (const QJsonValue &asset: asset_array) {
                if (asset["name"].toString() == platform_asset_name) {
                    download_url_ = asset["browser_download_url"].toString();
                    break;
                }
            }

            // The installer needs the file itself, so a missing asset is fatal
            // there. A platform that only opens the download can fall back to the
            // release page instead, which keeps a future rename from silencing
            // the check outright.
            const bool can_fall_back_to_page = !ENABLE_UPDATER && !release_page_url_.isEmpty();

            if (download_url_.isEmpty() && !can_fall_back_to_page) {
                report_error(tr("Can't find the download link of newest update for your platform!"));
            } else {
                changelog_request();
            }
        } else {
            report_error(tr(INVALID_JSON_ERR_STR));
        }
    } else {
        report_error(tr("Failed to get update download link from GitHub!"));
    }
}

void update_dialog::tag_download_link_request() {
    connect(access_manager_, &QNetworkAccessManager::finished, this, &update_dialog::on_tag_download_link_request_complete);

    QUrl tag_release_info_url_obj(QString(TAG_RELEASE_INFO_REQUEST_GET_URL).arg(UPDATE_TAG_NAME));
    QNetworkRequest tag_release_info_request(tag_release_info_url_obj);
    tag_release_info_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    access_manager_->get(tag_release_info_request);
}

void update_dialog::on_changelog_request_complete(QNetworkReply *reply) {
    access_manager_->disconnect(this);
    reply->deleteLater();

    ui_->changelog_textedit->setText(tr("Error retrieving changelog from GitHub..."));

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray reply_data = reply->readAll();

        QJsonParseError parse_error;
        QJsonDocument doc = QJsonDocument::fromJson(reply_data, &parse_error);

        if (doc.isObject()) {
            ui_->changelog_textedit->clear();

            QJsonArray commit_array = doc["commits"].toArray();

            for (const QJsonValue &commit: commit_array) {
                QJsonValue value_arr = commit["commit"].toObject();
                QString author = value_arr["author"].toObject()["name"].toString();
                QString message = value_arr["message"].toString();

                ui_->changelog_textedit->append(QString("- %1 (%2)\n").arg(message, author));
            }
        }
    }

    ui_->update_button->setDisabled(false);
    ui_->changelog_label->setText(tr("Changelog (update commit: %1)").arg(QString::fromStdString(new_commit_hash_)));

    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setVisible(true);
}

void update_dialog::changelog_request() {
    connect(access_manager_, &QNetworkAccessManager::finished, this, &update_dialog::on_changelog_request_complete);

    QString compare_req_url = QString(COMMITS_COMPARE_REQUEST_GET_URL).arg(QString(GIT_COMMIT_HASH), UPDATE_TAG_NAME);
    QUrl changelog_url_obj(QUrl::fromEncoded(compare_req_url.toUtf8()));
    QNetworkRequest changelog_req(changelog_url_obj);
    changelog_req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    access_manager_->get(changelog_req);
}

void update_dialog::check_for_update(const bool explicit_request) {
#if !ENABLE_UPDATE_CHECK
    if (explicit_request) {
        QMessageBox::information(this, tr("Updater unsupported"), tr("The updater is not yet supported on your platform!"));
    }

    // Unconditional: an automatic check used to return here without closing,
    // and a dialog that never closes is never deleted (WA_DeleteOnClose).
    close();
    return;
#else
    if (strcmp(GIT_COMMIT_HASH, "Stable") == 0) {
        close();
        return;
    }

    if (!explicit_request) {
        if (ui_->manual_check_checkbox->isChecked()) {
            close();
            return;
        }
    }

    explicit_up_ = explicit_request;

    ui_->update_button->setDisabled(true);
    ui_->ignore_button->show();
    ui_->cancel_button->hide();
    ui_->download_progress_bar->hide();

    setWindowFlags(windowFlags() | Qt::WindowCloseButtonHint);
    connect(access_manager_, &QNetworkAccessManager::finished, this, &update_dialog::on_tag_request_complete);

    QUrl tag_request_url_obj(QUrl::fromEncoded(QByteArray(TAG_REQUEST_GET_URL)));
    QNetworkRequest tag_request(tag_request_url_obj);
    tag_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    access_manager_->get(tag_request);
#endif
}

void update_dialog::on_new_update_download_progress_report(quint64 received, quint64 total) {
    ui_->download_progress_bar->setRange(0, total);
    ui_->download_progress_bar->setValue(received);
}

void update_dialog::on_update_button_clicked() {
#if !ENABLE_UPDATER
    // Nothing here can install over an AppImage or a mounted DMG, so open the
    // download and let the user install it the way they installed the emulator.
    QDesktopServices::openUrl(QUrl(download_url_.isEmpty() ? release_page_url_ : download_url_));
    close();
#else
    QDir().mkdir(UPDATE_FILE_STORE_FOLDER);
    downloaded_file_ = new QFile(UPDATE_FILE_STORE_PATH);
    if (!downloaded_file_->open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Update error"), tr("Can't not open temporary file for storing the update files!"));
        return;
    }

    ui_->ignore_button->hide();
    ui_->cancel_button->show();
    ui_->update_button->setDisabled(true);
    ui_->download_progress_bar->show();

    connect(access_manager_, &QNetworkAccessManager::finished, this, &update_dialog::on_new_update_download_finished);

    QUrl download_url_obj(QUrl::fromEncoded(download_url_.toUtf8()));
    QNetworkRequest download_request(download_url_obj);
    download_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    download_reply_ = access_manager_->get(download_request);

    connect(download_reply_, &QNetworkReply::downloadProgress, this, &update_dialog::on_new_update_download_progress_report);
    connect(download_reply_, &QNetworkReply::readyRead, this, &update_dialog::on_new_update_download_read_ready);
#endif
}

void update_dialog::on_ignore_button_clicked() {
    close();
}

void update_dialog::revert_ui_to_update_wait() {
    ui_->ignore_button->show();
    ui_->cancel_button->hide();
    ui_->update_button->setDisabled(false);
    ui_->download_progress_bar->setValue(0);
    ui_->download_progress_bar->hide();
}

void update_dialog::on_cancel_button_clicked() {
    if (download_reply_) {
        download_reply_->abort();
    }

    if (downloaded_file_) {
        downloaded_file_->close();
        downloaded_file_->remove();

        delete downloaded_file_;
        downloaded_file_ = nullptr;
    }

#if ENABLE_UPDATER
    QDir().remove(UPDATE_FILE_STORE_PATH);
#endif

    revert_ui_to_update_wait();
    close();
}

void update_dialog::on_new_update_download_read_ready() {
    if (download_reply_) {
        if (downloaded_file_) {
            downloaded_file_->write(download_reply_->readAll());
        }
    }
}

void update_dialog::update_encounter_error(const QString &error) {
    QMessageBox::critical(this, tr("Update error"), error);
    revert_ui_to_update_wait();
}

void update_dialog::on_manual_check_checkbox_toggled(bool checked) {
    QSettings settings;
    settings.setValue(NO_AUTO_CHECK_UPDATE_SETTING, checked);
}

void update_dialog::on_new_update_download_finished(QNetworkReply *reply) {
    access_manager_->disconnect(this);
    reply->deleteLater();

    if (downloaded_file_) {
        downloaded_file_->flush();
        downloaded_file_->close();

        delete downloaded_file_;
        downloaded_file_ = nullptr;
    }

    download_reply_ = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        update_encounter_error(tr("Failed to download update with error: %1").arg(reply->errorString()));
        return;
    } else {
        ui_->download_progress_bar->setValue(ui_->download_progress_bar->maximum());

#if EKA2L1_PLATFORM(WIN32)
        std::vector<eka2l1::common::archive_entry_info> entries;

        if (!eka2l1::common::list_archive(UPDATE_FILE_STORE_PATH, entries)) {
            update_encounter_error(tr("Downloaded update is not in zip format!"));
            return;
        }

        static const char *UPDATER_FOLDER = "updater";
        static const char *UPDATER_FILENAME = "updater.exe";
        static const char *UPDATER_EXEC_LAUNCH_PATH = "updater\\updater.exe";

        bool updater_patched = false;

        // Only the updater folder is unpacked here, straight over the running installation: the updater
        // is what performs the rest of the update, so it has to be the new build before it is launched.
        const bool unpacked = eka2l1::common::extract_archive(UPDATE_FILE_STORE_PATH,
            [&](const eka2l1::common::archive_entry_info &entry) -> std::string {
                if (entry.is_directory) {
                    return std::string();
                }

                const std::string head = entry.path.substr(0,
                    eka2l1::common::min(strlen(UPDATER_FOLDER), entry.path.length()));

                if (eka2l1::common::compare_ignore_case(head.c_str(), UPDATER_FOLDER) != 0) {
                    return std::string();
                }

                const std::string real_filename = eka2l1::filename(entry.path);

                if (eka2l1::common::compare_ignore_case(real_filename.c_str(), UPDATER_FILENAME) == 0) {
                    updater_patched = true;
                }

                return entry.path;
            },
            nullptr, nullptr);

        if (!unpacked) {
            update_encounter_error(tr("Failed to update the updater program!"));
            return;
        }

        if (!updater_patched) {
            update_encounter_error(tr("Can't find the updater program in the update archive file!"));
            return;
        }

        // Launch update process 
        QStringList arguments;
        arguments << QString::number(QCoreApplication::applicationPid());

        QProcess* update_process = new QProcess();
        update_process->setProgram(UPDATER_EXEC_LAUNCH_PATH);
        update_process->setArguments(arguments);
        update_process->setWorkingDirectory(QDir::currentPath() + "\\" + UPDATER_FOLDER);
        update_process->start(QIODevice::NotOpen);

        if (!update_process->waitForStarted()) {
            update_encounter_error(tr("Can't find the updater program in the update archive file!"));
            return;
        }
#endif

        emit exit_for_update_request();
        close();
    }
}
