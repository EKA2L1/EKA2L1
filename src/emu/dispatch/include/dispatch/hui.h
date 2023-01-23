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

#include <dispatch/def.h>
#include <mem/ptr.h>
#include <utils/des.h>
#include <utils/reqsts.h>

namespace eka2l1::dispatch {
    struct ehui_input_view_controller {
    private:
        std::u16string result_;
        epoc::notify_info info_;
    public:
        void on_input_view_complete(const std::u16string &result_text);
        void set_notify_info(epoc::notify_info info);

        std::u16string result_text() const {
            return result_;
        }

        std::size_t result_text_length() const {
            return result_.length();
        }
        
        bool is_another_input_dialog_active() const {
            return !info_.empty();
        }
    };

    struct ehui_controller {
    private:
        ehui_input_view_controller input_view_controller_;

    public:
        ehui_input_view_controller &input_view_controller() {
            return input_view_controller_;
        }
    };

    BRIDGE_FUNC_DISPATCHER(std::int32_t, ehui_open_input_view, epoc::desc16 *initial_text, const std::int32_t max_length, eka2l1::ptr<epoc::request_status> status);
    BRIDGE_FUNC_DISPATCHER(void, ehui_get_stored_input_text, std::int32_t *text_length, char16_t *text_ptr);
    BRIDGE_FUNC_DISPATCHER(void, ehui_close_input_view);
    BRIDGE_FUNC_DISPATCHER(bool, ehui_is_keypad_based);
}