/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <common/log.h>
#include <epoc/services/backup/backup.h>

namespace eka2l1 {
    backup_server::backup_server(eka2l1::system *sys)
        : service::server(sys, "!BackupServer", true) {
        REGISTER_IPC(backup_server, get_backup_operation_state, EBakOpCodeGetBackupOperationState,
            "Backup::GetOperationState");
    }

    void backup_server::get_backup_operation_state(service::ipc_context &ctx) {
        LOG_TRACE("GetBackupOperationState stubbed with false");

        bool state = false;

        ctx.write_arg_pkg<bool>(0, state);
        ctx.set_request_status(KErrNone);
    }
}
