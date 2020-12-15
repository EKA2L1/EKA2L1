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

#include <services/ui/plugins/common.h>
#include <services/ui/plugins/keylocknof.h>
#include <kernel/kernel.h>

#include <utils/err.h>

#include <common/log.h>
#include <common/chunkyseri.h>

namespace eka2l1::epoc::notifier {
    void keylock_plugin::handle(epoc::desc8 *request, epoc::des8 *respone, epoc::notify_info &complete_info) {
        if (!respone) {
            complete_info.complete(epoc::error_argument);
            return;
        }

        kernel::process *pr_req = complete_info.requester->owning_process();

        std::uint8_t *data_ptr = reinterpret_cast<std::uint8_t*>(request->get_pointer(pr_req));
        std::uint32_t data_size = request->get_length();
        
        std::uint8_t *given_respone = reinterpret_cast<std::uint8_t*>(respone->get_pointer(pr_req));
        std::uint32_t given_respone_max = request->get_max_length(pr_req);

        if (!data_ptr || !data_size || !given_respone || !given_respone_max) {
            complete_info.complete(epoc::error_argument);
            return;
        }

        common::chunkyseri seri(data_ptr, data_size, common::chunkyseri_mode::SERI_MODE_READ);
        common::chunkyseri seri_write(given_respone, given_respone_max, common::chunkyseri_mode::SERI_MODE_WRITE);

        std::uint32_t signature = 0;

        if (!kern_->is_eka1()) {
            seri.absorb(signature);

            if (signature != AKN_NOTIFIER_SIGNATURE) {
                complete_info.complete(epoc::error_argument);
                return;
            }

            seri_write.absorb(signature);
        }

        std::uint32_t reason = 0;

        seri.absorb(reason);
        seri_write.absorb(reason);

        std::int32_t enabled_bval = false;
        seri.absorb(enabled_bval);

        switch (reason) {
        case keylock_plugin_call_reason_inquire:
            enabled_bval = static_cast<std::int32_t>(locked_);
            break;

        case keylock_plugin_call_reason_enable:
            locked_ = true;
            enabled_bval = true;

            break;

        case keylock_plugin_call_reason_disable:
            locked_ = false;
            enabled_bval = false;

            break;

        default:
            LOG_WARN(SERVICE_UI, "Unimplemented keylock notifier reason handling: {}", reason);
        }

        seri_write.absorb(enabled_bval);
        complete_info.complete(epoc::error_none);
    }

    void keylock_plugin::cancel() {
    }
}