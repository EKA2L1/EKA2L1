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

#include <drivers/input/common.h>

#include <qt/state.h>
#include <qt/thread.h>
#include <qt/utils.h>

#include <common/fileutils.h>
#include <common/path.h>
#include <common/platform.h>
#include <common/log.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <QTranslator>

#include <iostream>
#include <memory>

#if EKA2L1_PLATFORM(UNIX)
// The AppImage bundles Qt's GStreamer backend but none of the plugins it needs,
// leaving Qt Multimedia with no camera. Its FFmpeg backend is bundled whole, so
// prefer that inside an AppImage. An explicit choice still wins.
static void prefer_selfcontained_media_backend() {
    if (!qEnvironmentVariableIsEmpty("QT_MEDIA_BACKEND")) {
        return;
    }

    if (qEnvironmentVariableIsEmpty("APPIMAGE") && qEnvironmentVariableIsEmpty("APPDIR")) {
        return;
    }

    qputenv("QT_MEDIA_BACKEND", "ffmpeg");
}
#endif

int main(int argc, char *argv[]) {
#if EKA2L1_PLATFORM(UNIX)
    prefer_selfcontained_media_backend();
#endif

    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("EKA2L1");
    QCoreApplication::setApplicationName("EKA2L1");

    // Everything below opens files in the data folder, so --data-dir is read
    // here; the rest of the command line is handled once the emulator is up.
    QString data_dir_option;
    const QStringList arguments = QCoreApplication::arguments();
    const qsizetype data_dir_index = arguments.indexOf("--data-dir");

    if (data_dir_index >= 0) {
        const QString value = (data_dir_index + 1 < arguments.size()) ? arguments[data_dir_index + 1] : QString();

        if (value.isEmpty()) {
            std::cerr << "--data-dir needs a folder" << std::endl;
            return -1;
        }

        // Most likely the folder was left out and the next option taken for it.
        if (value.startsWith('-')) {
            std::cerr << "--data-dir needs a folder, not the option " << value.toStdString()
                      << " (write ./" << value.toStdString() << " for a folder of that name)" << std::endl;
            return -1;
        }

        data_dir_option = QDir(value).absolutePath() + "/";

        // Keep the frontend's own settings with the rest of this instance's data.
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, data_dir_option);
    }

    QTranslator translator;
    QSettings settings;

    QVariant language_variant = settings.value(LANGUAGE_SETTING_NAME);
    bool lang_loaded = false;

    if (language_variant.isValid()) {
        const QString base_name = "eka2l1_" + language_variant.toString();
        if (translator.load(":/languages/" + base_name)) {
            a.installTranslator(&translator);
            lang_loaded = true;
        }
    }

    if (!lang_loaded) {
        const QStringList ui_languages = QLocale::system().uiLanguages();
        for (const QString &locale : ui_languages) {
            const QString locale_name = QLocale(locale).name();
            const QString base_name = "eka2l1_" + locale_name;

            if (translator.load(":/languages/" + base_name)) {
                a.installTranslator(&translator);
                settings.setValue(LANGUAGE_SETTING_NAME, locale_name);

                break;
            }
        }
    }
    
    qRegisterMetaType<std::vector<std::string>>("std::vector<std::string>");
    qRegisterMetaType<eka2l1::drivers::input_event>("eka2l1::drivers::input_event");

    QString data_path = data_dir_option;

#if !EKA2L1_PLATFORM(WIN32)
    if (data_path.isEmpty()) {
        data_path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/EKA2L1/";
    }
#endif

    if (!data_path.isEmpty()) {
        QDir root_dir = QDir::root();

        // Without the folder it was given, the instance could keep nothing: say so now
        // rather than fail later on the first file it opens there.
        if (!root_dir.mkpath(data_path) && !data_dir_option.isEmpty()) {
            std::cerr << "Cannot create the data folder " << data_path.toStdString() << std::endl;
            return -1;
        }

        std::string data_path_str = data_path.toUtf8().toStdString();

        QString app_path = QDir(QCoreApplication::applicationDirPath()).path();
        std::string app_path_str = app_path.toUtf8().toStdString();

        // A data folder next to the executable already holds what it ships. QDir
        // compares two folders that exist by their canonical paths, so the folder is
        // recognised when one of the two paths reaches it through a link, too.
        if (QDir(app_path) != QDir(data_path)) {
            eka2l1::common::copy_folder(app_path_str + "/patch", data_path_str + "/patch", 0, nullptr);
            eka2l1::common::copy_folder(app_path_str + "/resources", data_path_str + "/resources", 0, nullptr);

            // Keep shipped compatibility scripts current across application upgrades.
            // copy_folder merges into the destination, so separately named user scripts
            // remain untouched while updated bundled scripts replace stale copies.
            eka2l1::common::copy_folder(app_path_str + "/scripts", data_path_str + "/scripts", 0, nullptr);

            if (!eka2l1::common::exists(data_path_str + "/compat/")) {
                eka2l1::common::copy_folder(app_path_str + "/compat", data_path_str + "/compat", 0, nullptr);
            }
        }

        eka2l1::set_data_root(data_path_str);

        // The default folder has always been the working directory too, and a
        // relative path on the command line keeps meaning a path in there. With
        // --data-dir the working directory stays where the user started from.
        if (data_dir_option.isEmpty()) {
            eka2l1::common::set_current_directory(data_path_str);
        }
    }

    eka2l1::desktop::emulator emulator_state;
    return eka2l1::desktop::emulator_entry(a, emulator_state, argc, const_cast<const char **>(argv));
}
