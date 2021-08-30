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

#include <services/backup/backup.h>
#include <system/epoc.h>

#include <utils/err.h>

namespace eka2l1 {
    static const char *get_backup_server_name_through_epocver(const epocver ver) {
        if (ver < epocver::epoc81a) {
            return "BackupServer";
        }

        return "!BackupServer";
    }

    backup_server::backup_server(eka2l1::system *sys)
        : service::typical_server(sys, get_backup_server_name_through_epocver(sys->get_symbian_version_use())) {
    }

    void backup_server::connect(service::ipc_context &context) {
        create_session<backup_client_session>(&context);
        context.complete(epoc::error_none);
    }

    backup_client_session::backup_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version)
        , flags_(0) {
    }

    void backup_client_session::get_backup_operation_state(service::ipc_context *ctx) {
        LOG_TRACE(SERVICE_BACKUP, "GetBackupOperationState stubbed with false");

        bool state = false;

        ctx->write_data_to_descriptor_argument<bool>(0, state);
        ctx->complete(epoc::error_none);
    }

    void backup_client_session::set_backup_operation_observer_present(service::ipc_context *ctx) {
        flags_ |= FLAG_OBSERVER_PRESENT;
        ctx->complete(epoc::error_none);
    }

    void backup_client_session::backup_operation_ready(service::ipc_context *ctx) {
        if (!backup_operation_nof_.empty()) {
            ctx->complete(epoc::error_in_use);
            return;
        }

        backup_operation_nof_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
    }

    void backup_client_session::event_ready(service::ipc_context *ctx) {
        if (!event_operation_nof_.empty()) {
            ctx->complete(epoc::error_in_use);
            return;
        }

        event_operation_nof_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
    }

    void backup_client_session::notify_lock_change(service::ipc_context *ctx) {
        std::optional<std::u16string> filename = ctx->get_argument_value<std::u16string>(1);
        std::optional<std::uint32_t> flags = ctx->get_argument_value<std::uint32_t>(2);

        if (!filename.has_value() || !flags.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        for (std::size_t i = 0; i < lock_notify_requests_.size(); i++) {
            if (common::compare_ignore_case(lock_notify_requests_[i].filename_, filename.value()) == 0) {
                ctx->complete(epoc::error_already_exists);
                return;
            }
        }

        lock_notify_requests_.push_back(backup_lock_notify_request { filename.value(), flags.value() });
        ctx->complete(epoc::error_none);
    }

    void backup_client_session::fetch(service::ipc_context *ctx) {
        bool should_stub_complete = true;

        switch (ctx->msg->function) {
        case EBakOpCodeEventReady:
            event_ready(ctx);
            return;

        case EBakOpCodeNotifyLockChange:
            notify_lock_change(ctx);
            return;

        case EBakOpCodeSetBackupOperationObserverIsPresent:
            set_backup_operation_observer_present(ctx);
            return;

        case EBakOpCodeBackupOperationEventReady:
            backup_operation_ready(ctx);
            return;

        case EBakOpCodeGetBackupOperationState:
            get_backup_operation_state(ctx);
            return;

        default:
            break;
        }

        LOG_ERROR(SERVICE_BACKUP, "Unimplemented opcode for backup server server 0x{:X}", ctx->msg->function);
        //ctx->complete(epoc::error_none);
    }
}