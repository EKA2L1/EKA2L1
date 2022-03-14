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

#include <drivers/ui/input_dialog.h>
#include <qt/input_dialog_driver.h>
#include <qt/mainwindow.h>
#include <common/log.h>

namespace eka2l1::drivers::ui {
    main_window *main_window_instance;

    bool open_input_view(const std::u16string &initial_text, const int max_len, input_dialog_complete_callback complete_callback) {
        if (!main_window_instance) {
            LOG_TRACE(FRONTEND_UI, "Main window has not been initialized!");
            return true;
        }

        return main_window_instance->input_dialog_open(initial_text, max_len, complete_callback);
    }

    void close_input_view()  {
        if (!main_window_instance) {
            LOG_TRACE(FRONTEND_UI, "Main window has not been initialized!");
        }

        main_window_instance->input_dialog_close();
    }
}