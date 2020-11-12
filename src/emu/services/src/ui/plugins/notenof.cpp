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

#include <services/ui/plugins/notenof.h>
#include <kernel/kernel.h>

#include <utils/err.h>
#include <common/log.h>

namespace eka2l1::epoc::notifier {
    void note_display_plugin::handle(epoc::desc8 *request, epoc::des8 *respone, epoc::notify_info &complete_info) {
        if (outstanding_) {
            complete_info.complete(epoc::error_in_use);
            return;
        }

        kernel::process *pr_req = complete_info.requester->owning_process();

        std::uint8_t *data_ptr = reinterpret_cast<std::uint8_t*>(request->get_pointer(pr_req));
        std::uint32_t data_size = request->get_length();

        if (!data_ptr || !data_size) {
            complete_info.complete(epoc::error_argument);
            return;
        }

        common::chunkyseri seri(data_ptr, data_size, common::chunkyseri_mode::SERI_MODE_READ);

        if (kern_->is_eka1()) {
            std::uint16_t type = 0;
            char unk = 0;

            seri.absorb(type);
            seri.absorb(unk);

            std::string to_display(reinterpret_cast<const char*>(data_ptr + sizeof(std::uint16_t) + sizeof(char)));
            LOG_TRACE("Trying to display dialog type: {} with message: {}", type, to_display);

            if (callback_) {
                callback_(static_cast<note_type>(type), to_display, complete_info);
                outstanding_ = true;
            } else {
                complete_info.complete(epoc::error_none);
                outstanding_ = false;
            }
        } else {
            LOG_INFO("Note display currently unsupported for EKA2.");

            complete_info.complete(epoc::error_none);
            outstanding_ = false;
        }
    }

    void note_display_plugin::cancel() {
        if (!outstanding_) {
            return;
        }

        outstanding_ = false;
        if (cancel_callback_) {
            cancel_callback_();
        }
    }
}