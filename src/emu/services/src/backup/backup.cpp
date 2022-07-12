/*
 * Copyright (c) 2022 EKA2L1 Team
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
    backup_old_server::backup_old_server(eka2l1::system *sys)
        : service::typical_server(sys, "BackupServer") {
    }

    void backup_old_server::connect(service::ipc_context &context) {
        create_session<backup_old_session>(&context);
        context.complete(epoc::error_none);
    }

    backup_old_session::backup_old_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void backup_old_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case 0x1F:
        case 0x22:
            ctx->complete(epoc::error_none);
            break;

        default:
            LOG_ERROR(SERVICE_BACKUP, "Unimplemented opcode for Old Backup server 0x{:X}", ctx->msg->function);
            break;
        }
    }
}
