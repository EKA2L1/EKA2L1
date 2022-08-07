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

#include <qt/utils.h>

#include <config/app_settings.h>
#include <common/random.h>
#include <common/version.h>

#include <kernel/kernel.h>
#include <services/ui/cap/oom_app.h>
#include <services/window/classes/wingroup.h>
#include <services/window/window.h>
#include <system/epoc.h>

#include <QCheckBox>
#include <QSettings>

eka2l1::window_server *get_window_server_through_system(eka2l1::system *sys) {
    eka2l1::kernel_system *kernel = sys->get_kernel_system();
    if (!kernel) {
        return nullptr;
    }

    const std::string win_server_name = eka2l1::get_winserv_name_by_epocver(kernel->get_epoc_version());
    return reinterpret_cast<eka2l1::window_server *>(kernel->get_by_name<eka2l1::service::server>(win_server_name));
}

eka2l1::epoc::screen *get_current_active_screen(eka2l1::system *sys, const int provided_num) {
    int active_screen_number = provided_num;
    if (provided_num < 0) {
        QSettings settings;
        active_screen_number = settings.value(SHOW_SCREEN_NUMBER_SETTINGS_NAME, 0).toInt();
    }

    eka2l1::window_server *server = get_window_server_through_system(sys);
    if (server) {
        eka2l1::epoc::screen *scr = server->get_screens();
        while (scr) {
            if (scr->number == active_screen_number) {
                return scr;
            }

            scr = scr->next;
        }
    }

    return nullptr;
}

std::optional<eka2l1::akn_running_app_info> get_active_app_info(eka2l1::system *sys, const int provided_num) {
    eka2l1::epoc::screen *scr = get_current_active_screen(sys, provided_num);
    if (!scr || !scr->focus) {
        return std::nullopt;
    }

    eka2l1::epoc::window_group *group = scr->get_group_chain();
    std::optional<eka2l1::akn_running_app_info> best_info;

    while (group) {
        std::optional<eka2l1::akn_running_app_info> info = eka2l1::get_akn_app_info_from_window_group(group);
        if (info.has_value()) {
            if (group == scr->focus) {
                return info;
            }

            eka2l1::kernel::thread *own_thr = scr->focus->client->get_client();
            if (own_thr && (info->associated_->has_child_process(own_thr->owning_process()))) {
                best_info = info;
            }
        }

        group = reinterpret_cast<eka2l1::epoc::window_group *>(group->sibling);
    }

    return best_info;
}

eka2l1::config::app_setting *get_active_app_setting(eka2l1::system *sys, eka2l1::config::app_settings &settings, const int provided_num) {
    std::optional<eka2l1::akn_running_app_info> info = get_active_app_info(sys, provided_num);
    if (!info.has_value()) {
        return nullptr;
    }
    return settings.get_setting(info->app_uid_);
}

QMessageBox::StandardButton make_dialog_with_checkbox_and_choices(const QString &title, const QString &text, const QString &checkbox_text, const bool checkbox_state, dialog_checkbox_toggled_callback checkbox_callback, const bool two_choices) {
    QCheckBox *checkbox = new QCheckBox(checkbox_text);
    QMessageBox dialog;
    dialog.setWindowTitle(title);
    dialog.setText(text);
    dialog.setCheckBox(checkbox);
    dialog.setIcon(QMessageBox::Information);
    dialog.addButton(QMessageBox::Ok);

    if (two_choices)
        dialog.addButton(QMessageBox::Cancel);

    dialog.setDefaultButton(QMessageBox::Ok);
    checkbox->setChecked(checkbox_state);

    QObject::connect(checkbox, &QCheckBox::toggled, checkbox_callback);

    dialog.exec();
    return static_cast<QMessageBox::StandardButton>(dialog.result());
}

QString get_emulator_window_title() {
    QSettings settings;
    QString window_title = QObject::tr("EKA2L1 - Symbian OS emulator");

    if (!settings.value(STATIC_TITLE_SETTING_NAME, false).toBool()) {
        window_title = "EKA2L1 " CURRENT_EKA2L1_VERSION_STRING " (" GIT_BRANCH " " GIT_COMMIT_HASH ") - Symbian OS emulator";
    }

    return window_title;
}

QString epocver_to_symbian_readable_name(const epocver ver) {
    switch (ver) {
    case epocver::epocu6:
        return QString("epocu6");

    case epocver::epoc6:
        return QString("S60v1");

    case epocver::epoc80:
        return QString("S60v2 - 8.0");

    case epocver::epoc81a:
        return QString("S60v2 - 8.1a");

    case epocver::epoc81b:
        return QString("S60v2 - 8.1b");

    case epocver::epoc93fp1:
        return QString("S60v3 FP1");

    case epocver::epoc93fp2:
        return QString("S60v3 FP2");

    case epocver::epoc94:
        return QString("S60v5");

    case epocver::epoc95:
        return QString("S^3");

    case epocver::epoc10:
        return QString("Belle");

    default:
        break;
    }

    return QString("Unknown");
}
