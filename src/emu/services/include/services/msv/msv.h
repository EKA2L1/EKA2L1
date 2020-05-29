/*
 * Copyright (c) 2020 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
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

#include <services/msv/common.h>
#include <services/msv/entry.h>
#include <services/msv/registry.h>

#include <services/framework.h>
#include <kernel/server.h>

#include <utils/des.h>
#include <utils/reqsts.h>

#include <memory>
#include <queue>

namespace eka2l1 {
    enum msv_opcode {
        msv_get_entry = 0x0,
        msv_get_children = 0x1,
        msv_notify_session_event = 0xB,
        msv_cancel_notify_session_event = 0xC,
        msv_mtm_group_ref = 0x1C,
        msv_mtm_group_unref = 0x1D,
        msv_fill_registered_mtm_dll_array = 0x19,
        msv_get_message_directory = 0x25,
        msv_set_as_observer_only = 0x27,
        msv_get_notify_sequence = 0x2D,
        msv_set_receive_entry_events = 0x31,
        msv_get_message_drive = 0x33
    };

    class msv_server : public service::typical_server {
        friend struct msv_client_session;

        std::u16string message_folder_;
        epoc::msv::mtm_registry reg_;

        std::unique_ptr<epoc::msv::entry_indexer> indexer_;

        bool inited_;

    protected:
        void install_rom_mtm_modules();
        void init();

    public:
        explicit msv_server(eka2l1::system *sys);

        const std::u16string message_folder() const {
            return message_folder_;
        }

        void connect(service::ipc_context &context) override;
    };

    struct msv_event_data {
        epoc::msv::change_notification_type nof_;
        std::uint32_t arg1_;
        std::uint32_t arg2_;
        std::string selection_;
    };

    struct msv_client_session : public service::typical_session {
        epoc::notify_info msv_info_;
        epoc::des8 *change_;
        epoc::des8 *selection_;

        std::queue<msv_event_data> events_;
        std::uint32_t nof_sequence_;

        enum msv_session_flag {
            FLAG_RECEIVE_ENTRY_EVENTS = 1 << 0,
            FLAG_OBSERVER_ONLY = 1 << 1
        };

        std::uint32_t flags_;

        std::vector<epoc::msv::entry *> child_entries_;

    protected:
        bool listen(epoc::notify_info &info, epoc::des8 *change, epoc::des8 *sel);

    public:
        explicit msv_client_session(service::typical_server *serv, const std::uint32_t ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void notify_session_event(service::ipc_context *ctx);
        void cancel_notify_session_event(service::ipc_context *ctx);
        void get_message_directory(service::ipc_context *ctx);
        void get_message_drive(service::ipc_context *ctx);
        void set_receive_entry_events(service::ipc_context *ctx);
        void fill_registered_mtm_dll_array(service::ipc_context *ctx);
        void ref_mtm_group(service::ipc_context *ctx);
        void unref_mtm_group(service::ipc_context *ctx);
        void get_entry(service::ipc_context *ctx);
        void get_children(service::ipc_context *ctx);
        void get_notify_sequence(service::ipc_context *ctx);
        void set_as_observer_only(service::ipc_context *ctx);

        void queue(msv_event_data &evt);
    };
}
