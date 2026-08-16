/*
 * Copyright (c) 2026 EKA2L1 Team.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <common/applauncher.h>
#include <drivers/ui/input_dialog.h>

namespace eka2l1::common {
    bool launch_browser(const std::string &) {
        return false;
    }
}

namespace eka2l1::drivers::ui {
    bool open_input_view(const std::u16string &, const int, input_dialog_complete_callback) {
        return false;
    }

    void close_input_view() {
    }

    void show_yes_no_dialog(const std::u16string &, const std::u16string &, const std::u16string &,
        yes_no_dialog_complete_callback complete_callback) {
        if (complete_callback) {
            complete_callback(1);
        }
    }
}
