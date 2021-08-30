/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <kernel/server.h>
#include <services/framework.h>
#include <utils/reqsts.h>

#include <string>

namespace eka2l1 {
    enum TBaBakOpCode {
        EBakOpCodeEventReady = 20, // EBakOpCodeStartNotifications,
        EBakOpCodeGetEvent,
        EBakOpCodeCloseAllFiles,
        EBakOpCodeRestartAll,
        EBakOpCodeCloseFile,
        EBakOpCodeRestartFile,
        EBakOpCodeNotifyLockChange,
        EBakOpCodeNotifyLockChangeCancel,
        EBakOpCodeCloseServer, // no longer supported
        EBakOpCodeNotifyBackupOperation,
        EBakOpCodeCancelOutstandingBackupOperationEvent,
        EBakOpCodeGetBackupOperationState,
        EBakOpCodeBackupOperationEventReady,
        EBakOpCodeGetBackupOperationEvent,
        EBakOpCodeSetBackupOperationObserverIsPresent,
        EBakOpCodeStopNotifications
    };

    class backup_server : public service::typical_server {
    public:
        explicit backup_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
    };

    struct backup_lock_notify_request {
        std::u16string filename_;
        std::uint32_t flags_;
    };

    struct backup_client_session : public service::typical_session {
    private:
        enum {
            FLAG_OBSERVER_PRESENT = 1 << 0
        };

        std::uint32_t flags_;

        epoc::notify_info backup_operation_nof_;
        epoc::notify_info event_operation_nof_;

        std::vector<backup_lock_notify_request> lock_notify_requests_;

    public:
        explicit backup_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void get_backup_operation_state(service::ipc_context *ctx);
        void set_backup_operation_observer_present(service::ipc_context *ctx);
        void backup_operation_ready(service::ipc_context *ctx);
        void event_ready(service::ipc_context *ctx);
        void notify_lock_change(service::ipc_context *ctx);
    };
}
