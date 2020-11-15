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

#include <system/epoc.h>
#include <services/backup/backup.h>

#include <utils/err.h>

namespace eka2l1 {
    static const char *get_backup_server_name_through_epocver(const epocver ver) {
        if (ver <= epocver::eka2) {
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
        : service::typical_session(serv, ss_id, client_version) {
    }

    void backup_client_session::get_backup_operation_state(service::ipc_context *ctx) {
        LOG_TRACE("GetBackupOperationState stubbed with false");

        bool state = false;

        ctx->write_data_to_descriptor_argument<bool>(0, state);
        ctx->complete(epoc::error_none);
    }

    void backup_client_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case EBakOpCodeGetBackupOperationState:
            get_backup_operation_state(ctx);
            return;

        default:
            break;
        }

        LOG_ERROR("Unimplemented opcode for backup server server 0x{:X}", ctx->msg->function);
        ctx->complete(epoc::error_none);
    }
}