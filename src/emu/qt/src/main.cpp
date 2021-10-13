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

#include <QApplication>
#include <QFile>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

#include <memory>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("EKA2L1");
    QCoreApplication::setApplicationName("EKA2L1");

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

    eka2l1::desktop::emulator emulator_state;
    return eka2l1::desktop::emulator_entry(a, emulator_state, argc, const_cast<const char **>(argv));
}
