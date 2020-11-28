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
    enum keylock_plugin_call_reason {
        keylock_plugin_call_reason_enable = 0,
        keylock_plugin_call_reason_disable = 1,
        keylock_plugin_call_reason_allow_nofs = 2,
        keylock_plugin_call_reason_disable_nofs = 3,
        keylock_plugin_call_reason_inquire = 4,
        keylock_plugin_call_reason_offer = 5,
        keylock_plugin_call_reason_cancel_all_nofs = 6,
        keylock_plugin_call_reason_enable_auto_lock_emu = 7,
        keylock_plugin_call_reason_disable_without_note = 8,
        keylock_plugin_call_reason_enable_without_note = 9
    };

    class keylock_plugin : public plugin_base {
        bool locked_;

    public:
        explicit keylock_plugin(kernel_system *kern)
            : plugin_base(kern)
            , locked_(false) {
        }

        ~keylock_plugin() override {
        }

        epoc::uid unique_id() const override {
            return 0x100059B0;
        }

        void handle(epoc::desc8 *request, epoc::des8 *respone, epoc::notify_info &complete_info) override;
        void cancel() override;
    };
};