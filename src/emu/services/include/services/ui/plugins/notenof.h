/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <services/notifier/plugin.h>
#include <functional>
#include <string>

namespace eka2l1::epoc::notifier {
    // Reference to AknNotifyStd
    enum note_type {
        note_type_info = 1,
        note_type_warning = 2,
        note_type_confirmation = 3,
        note_type_error = 4,
        note_type_charging = 5,
        note_type_wait = 6,
        note_type_permanent = 7,
        note_type_not_charging = 8,
        note_type_battery_full = 9,
        note_type_battery_low = 10,
        note_type_recharge_battery = 11,
        note_type_cancel = 12,
        note_type_text = 13
    };

    using note_display_callback = std::function<void(const note_type, const std::string&, epoc::notify_info)>;
    using note_display_cancel_callback = std::function<void()>;

    class note_display_plugin: public plugin_base {
        note_display_callback callback_;
        note_display_cancel_callback cancel_callback_;

        bool outstanding_;

    public:
        explicit note_display_plugin(kernel_system *kern)
            : plugin_base(kern)
            , callback_(nullptr)
            , cancel_callback_(nullptr)
            , outstanding_(false) {
        }

        void set_note_display_callback(note_display_callback callback) {
            callback_ = callback;
        }

        void set_note_display_cancel_callback(note_display_cancel_callback callback) {
            cancel_callback_ = callback;
        }

        epoc::uid unique_id() const override {
            return 0x100059B1;
        }

        void handle(epoc::desc8 *request, epoc::des8 *respone, epoc::notify_info &complete_info) override;
        void cancel() override;
    };
};