/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <common/log.h>
#include <dispatch/dispatcher.h>
#include <dispatch/hui.h>
#include <drivers/ui/input_dialog.h>
#include <kernel/thread.h>
#include <kernel/kernel.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1::dispatch {
    void ehui_input_view_controller::set_notify_info(epoc::notify_info info) {
        info_ = std::move(info);
        result_.clear();
    }
    
    void ehui_input_view_controller::on_input_view_complete(const std::u16string &result_text) {
        result_ = result_text;

        if (!info_.empty()) {
            kernel_system *kern = info_.requester->get_kernel_object_owner();

            kern->lock();
            info_.complete(epoc::error_none);
            kern->unlock();
        }
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, ehui_open_input_view, epoc::desc16 *initial_text, const std::int32_t max_length, eka2l1::ptr<epoc::request_status> status) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        kernel_system *kern = sys->get_kernel_system();

        ehui_input_view_controller &controller = dispatcher->get_hui_controller().input_view_controller();

        if (controller.is_another_input_dialog_active()) {
            return epoc::error_in_use;
        }

        controller.set_notify_info(epoc::notify_info(status, kern->crr_thread()));
        if (!drivers::ui::open_input_view(initial_text->to_std_string(kern->crr_process()), max_length, [&controller](const std::u16string &result) {
            controller.on_input_view_complete(result);
        })) {
            return epoc::error_in_use;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(void, ehui_get_stored_input_text, std::int32_t *text_length, char16_t *text_ptr) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        kernel_system *kern = sys->get_kernel_system();
        ehui_input_view_controller &controller = dispatcher->get_hui_controller().input_view_controller();

        if (text_ptr == nullptr) {
            *text_length = static_cast<std::int32_t>(controller.result_text_length());
            return;
        }

        std::u16string text_string = controller.result_text();
        std::memcpy(text_ptr, text_string.data(), text_string.length() * sizeof(char16_t));
    }

    BRIDGE_FUNC_DISPATCHER(void, ehui_close_input_view) {
        drivers::ui::close_input_view();
    }
}
