/*
 * Copyright (c) 2021 EKA2L1 Team
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

#include <services/msv/operations/move.h>

#include <services/msv/common.h>
#include <services/msv/entry.h>
#include <services/msv/msv.h>

#include <common/chunkyseri.h>
#include <utils/err.h>

namespace eka2l1::epoc::msv {
    move_operation::move_operation(const msv_id operation_id, const operation_buffer &buffer,
        epoc::notify_info complete_info)
        : operation(operation_id, buffer, complete_info) {
    }

    void move_operation::execute(msv_server *server, const kernel::uid process_uid) {
        state(operation_state_pending);

        std::vector<std::uint32_t> ids;
        std::uint32_t param1 = 0;
        std::uint32_t param2 = 0;

        common::chunkyseri seri(buffer_.data(), buffer_.size(), common::SERI_MODE_READ);
        absorb_command_data(seri, ids, param1, param2);

        epoc::msv::entry_indexer *indexer = server->indexer();

        msv_event_data moved_event;

        moved_event.nof_ = epoc::msv::change_notification_type_entries_moved;
        moved_event.arg1_ = param1;
        moved_event.arg2_ = 0;

        bool flushed = false;

        local_operation_progress *prog = progress_data<local_operation_progress>();
        prog->operation_ = local_operation_move;
        prog->number_of_entries_ = static_cast<std::uint32_t>(ids.size());
        prog->error_ = 0;
        prog->number_completed_ = 0;
        prog->number_failed_ = 0;
        prog->number_remaining_ = static_cast<std::uint32_t>(ids.size());

        io_system *io = server->get_io_system();
        
        for (std::size_t i = 0; i < ids.size(); i++) {
            epoc::msv::entry *ent = indexer->get_entry(ids[i]);
            prog->mid_ = ent->id_;

            if (!ent) {
                LOG_ERROR(SERVICE_SMS, "Entry id 0x{:X} does not exist to be sent", ids[i]);

                prog->number_failed_++;
                prog->number_remaining_--;

                prog->error_ = epoc::error_general;

                continue;
            }

            const epoc::msv::msv_id old_parent = ent->parent_id_;

            if (!server->move_entry(ids[i], param1)) {
                LOG_ERROR(SERVICE_SMS, "Fail to move entry to sent inbox");

                prog->number_failed_++;
                prog->number_remaining_--;

                prog->error_ = epoc::error_general;

                continue;
            } else {
                ent = indexer->get_entry(ids[i]);

                if ((moved_event.arg2_ == 0) || (moved_event.arg2_ == old_parent)) {
                    flushed = false;
                } else {
                    flushed = true;
                    server->queue(moved_event);

                    moved_event.ids_.clear();
                }

                moved_event.arg2_ = old_parent;
                moved_event.ids_.push_back(ids[i]);

                prog->number_completed_++;
                prog->number_remaining_--;
                prog->error_ = epoc::error_none;
            }
        }

        if (!flushed) {
            server->queue(moved_event);
        }

        state(operation_state_success);
        complete_info_.complete(epoc::error_none);
    }

    std::int32_t move_operation::system_progress(system_progress_info &progress) {
        local_operation_progress *localprg = progress_data<local_operation_progress>();

        progress.err_code_ = localprg->error_;
        progress.id_ = localprg->mid_;

        return epoc::error_none;
    }
}