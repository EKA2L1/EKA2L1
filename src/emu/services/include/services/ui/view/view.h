/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <services/framework.h>
#include <services/ui/view/common.h>
#include <services/ui/view/queue.h>

#include <vector>

namespace eka2l1 {
    class io_system;

    namespace kernel {
        class thread;
    }

    enum view_opcode {
        view_opcode_first = 0,
        view_opcode_create = 1,
        view_opcode_add_view = 2,
        view_opcode_remove_view = 3,
        view_opcode_request_view_event = 4,
        view_opcode_request_view_event_cancel = 5,
        view_opcode_active_view = 6,
        view_opcode_request_custom_message = 7,
        view_opcode_start_app = 8,
        view_opcode_deactivate_active_view = 9,
        view_opcode_notify_next_deactivation = 10,
        view_opcode_notify_next_activation = 11,
        view_opcode_create_activate_view_event = 12,
        view_opcode_create_deactivate_view_event = 13,
        view_opcode_get_system_default_view = 14,
        view_opcode_check_source_of_view_switch = 15,
        view_opcode_async_msg_for_client_to_panic = 16,
        view_opcode_set_protected = 17,
        view_opcode_set_cross_check_uid = 18,
        view_opcode_deactivate_active_view_if_owner_match = 19,
        view_opcode_priority = 20,
        view_opcode_set_background_color = 21,
        view_opcode_current_active_view_id = 22
    };

    using custom_message = std::vector<std::uint8_t>;
    std::string get_view_server_name_by_epocver(const epocver ver);

    class view_session : public service::typical_session {
        ipc_msg_ptr to_panic_;
        ui::view::event_queue queue_;
        epoc::uid app_uid_;

        std::queue<custom_message> customs_;

    public:
        void async_message_for_client_to_panic_with(service::ipc_context *ctx);
        void get_priority(service::ipc_context *ctx);
        void add_view(service::ipc_context *ctx);
        void request_view_event(service::ipc_context *ctx);
        void active_view(service::ipc_context *ctx, const bool /*should_complete*/);
        void deactive_view(service::ipc_context *ctx, const bool /*should_complete*/);

        explicit view_session(service::typical_server *server, const kernel::uid session_uid, epoc::version client_version);
        void fetch(service::ipc_context *ctx) override;
    };

    class view_server : public service::typical_server {
        std::uint32_t priority_;
        std::uint8_t flags_;

        enum flags {
            flag_inited = 1 << 0
        };

        ui::view::view_id active_;
        std::vector<ui::view::view_id> ids_;

    public:
        explicit view_server(system *sys);
        void connect(service::ipc_context &ctx) override;

        bool init(io_system *io);

        const std::uint32_t priority() const {
            return priority_;
        }

        ui::view::view_id active_view() const {
            return active_;
        }

        void set_active(ui::view::view_id new_id) {
            active_ = new_id;
        }

        void deactivate();
        void add_view(const ui::view::view_id &new_id);
    };
};