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

#pragma once

#include <optional>
#include <services/ui/cap/oom_app.h>

#include <QMessageBox>

namespace eka2l1 {
    class window_server;
    class system;

    namespace config {
        struct app_setting;
        class app_settings;
    }

    namespace epoc {
        struct screen;
        struct window_group;
    }
}

static constexpr const char *RECENT_MOUNT_SETTINGS_NAME = "recentMountFolders";
static constexpr const char *SHOW_SCREEN_NUMBER_SETTINGS_NAME = "showScreenNumber";
static constexpr const char *LANGUAGE_SETTING_NAME = "activeUILanguage";
static constexpr const char *STATIC_TITLE_SETTING_NAME = "useStaticTitle";
static constexpr const char *RECENT_BANK_FOLDER_SETTING_NAME = "recentBankFolder";
static constexpr const char *SHOW_LOG_CONSOLE_SETTING_NAME = "showLogConsole";

typedef void (*dialog_checkbox_toggled_callback)(bool toggled);

eka2l1::window_server *get_window_server_through_system(eka2l1::system *sys);
eka2l1::epoc::screen *get_current_active_screen(eka2l1::system *sys, const int provided_num = -1);
eka2l1::config::app_setting *get_active_app_setting(eka2l1::system *sys, eka2l1::config::app_settings &settings, const int provided_num = -1);

std::optional<eka2l1::akn_running_app_info> get_active_app_info(eka2l1::system *sys, const int provided_num = -1);
QMessageBox::StandardButton make_dialog_with_checkbox_and_choices(const QString &title, const QString &text, const QString &checkbox_text, const bool checkbox_state, dialog_checkbox_toggled_callback checkbox_callback, const bool two_choices);
QString get_emulator_window_title();
QString epocver_to_symbian_readable_name(const epocver ver);
