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

#include <services/sms/mtm/send.h>
#include <services/msv/common.h>
#include <services/msv/entry.h>
#include <services/msv/msv.h>
#include <services/msv/store.h>
#include <services/sms/common.h>
#include <services/msv/operations/base.h>

#include <common/chunkyseri.h>
#include <utils/err.h>

#include <vfs/vfs.h>

namespace eka2l1::epoc::sms {
    schedule_copy_operation::schedule_copy_operation(const epoc::msv::msv_id operation_id, const epoc::msv::operation_buffer &buffer,
        epoc::notify_info complete_info) 
        : epoc::msv::operation(operation_id, buffer, complete_info) {

    }

    void schedule_copy_operation::execute(msv_server *server, const kernel::uid process_uid) {
        // Get all the things that want to be sent
        state(epoc::msv::operation_state_pending);

        std::vector<std::uint32_t> ids;
        std::uint32_t param1 = 0;
        std::uint32_t param2 = 0;

        common::chunkyseri seri(buffer_.data(), buffer_.size(), common::SERI_MODE_READ);
        absorb_command_data(seri, ids, param1, param2);

        LOG_INFO(SERVICE_SMS, "Message schedule copy send stubbed to success");

        epoc::msv::entry_indexer *indexer = server->indexer();

        msv_event_data moved_event;

        moved_event.nof_ = epoc::msv::change_notification_type_entries_moved;
        moved_event.arg1_ = epoc::msv::MSV_SENT_ENTRY_ID_VALUE;
        moved_event.arg2_ = 0;

        bool flushed = false;

        epoc::sms::progress *sms_prog = progress_data<epoc::sms::progress>();
        sms_prog->type_ = epoc::sms::progress::progress_type_scheduling;
        sms_prog->error_ = 0;
        sms_prog->msg_count_ = static_cast<std::int32_t>(ids.size());
        sms_prog->msg_done_ = 0;
        sms_prog->state_ = epoc::msv::send_state_sending;

        io_system *io = server->get_io_system();
        
        for (std::size_t i = 0; i < ids.size(); i++) {
            epoc::msv::entry *ent = indexer->get_entry(ids[i]);
            if (!ent) {
                LOG_ERROR(SERVICE_SMS, "Entry id 0x{:X} does not exist to be sent", ids[i]);
                continue;
            }

            const epoc::msv::msv_id old_parent = ent->parent_id_;

            if (!server->move_entry(ids[i], epoc::msv::MSV_SENT_ENTRY_ID_VALUE)) {
                LOG_ERROR(SERVICE_SMS, "Fail to move entry to sent inbox");

                sms_prog->state_ = epoc::msv::send_state_failed;
                sms_prog->error_ = epoc::error_permission_denied;

                sys_progress_.err_code_ = sms_prog->error_;
                sys_progress_.id_ = ids[i];

                break;
            } else {
                ent = indexer->get_entry(ids[i]);

                sys_progress_.err_code_ = epoc::error_none;
                sys_progress_.id_ = ids[i];

                // Edit the store of this message, to indicates the destination address and send state
                std::optional<std::u16string> target_file_path = indexer->get_entry_data_file(ent);
                if (target_file_path.has_value() && io->exist(target_file_path.value())) {
                    symfile target_file_obj = io->open_file(target_file_path.value(), READ_MODE | BIN_MODE);
                    
                    if (target_file_obj) {
                        eka2l1::ro_file_stream target_file_read_stream(target_file_obj.get());
                        msv::store target_store;

                        if (!target_store.read(target_file_read_stream) || !target_store.buffer_exists(MSV_SMS_HEADER_STREAM_UID)) {
                            LOG_ERROR(SERVICE_SMS, "Unable to read store of id 0x{:X} for sent change", ids[i]);
                            goto QUEUE_EVENT;
                        }

                        msv::store_buffer &buffer = target_store.buffer_for(MSV_SMS_HEADER_STREAM_UID);
                        common::chunkyseri seri(buffer.data(), buffer.size(), common::SERI_MODE_READ);

                        sms_header target_header;
                        if (!target_header.absorb(seri)) {
                            LOG_ERROR(SERVICE_SMS, "Unable to load SMS header of id 0x{:X}!", ids[i]);
                            goto QUEUE_EVENT;
                        }

                        target_header.message_.store_status_ = mobile_store_status_sent;
                        if (!target_header.message_.pdu_ || (target_header.message_.pdu_->pdu_type_ != sms_pdu_type_submit)) {
                            LOG_ERROR(SERVICE_SMS, "Unsupported PDU type {} for id 0x{:X}!", target_header.message_.pdu_ ? target_header.message_.pdu_->pdu_type_ : 0, ids[i]);
                            goto QUEUE_EVENT;
                        }

                        sms_submit *submit_pdu = reinterpret_cast<sms_submit*>(target_header.message_.pdu_.get());
                        if (submit_pdu->dest_addr_.buffer_.empty()) {
                            if (target_header.recipients_.empty()) {
                                LOG_ERROR(SERVICE_SMS, "No recipient to sent to for id 0x{:X}!", ids[i]);
                                goto QUEUE_EVENT;
                            }

                            submit_pdu->dest_addr_.buffer_ = target_header.recipients_[0].number_;
                        }

                        seri = common::chunkyseri(nullptr, 0, common::SERI_MODE_MEASURE);
                        target_header.absorb(seri);

                        buffer.resize(seri.size());

                        seri = common::chunkyseri(buffer.data(), buffer.size(), common::SERI_MODE_WRITE);
                        target_header.absorb(seri);

                        target_file_obj->close();
                        target_file_obj = io->open_file(target_file_path.value(), WRITE_MODE | BIN_MODE);

                        if (!target_file_obj) {
                            LOG_ERROR(SERVICE_SMS, "Can't reopen store 0x{:X} for write!", ids[i]);
                            goto QUEUE_EVENT;
                        }

                        eka2l1::wo_file_stream target_file_write_stream(target_file_obj.get());
                        if (!target_store.write(target_file_write_stream)) {
                            LOG_ERROR(SERVICE_SMS, "Fail to externalize store 0x{:X}", ids[i]);
                        }

                        target_file_obj->flush();
                        target_file_obj->close();
                    }
                }

QUEUE_EVENT:

                if ((moved_event.arg2_ == 0) || (moved_event.arg2_ == old_parent)) {
                    flushed = false;
                } else {
                    flushed = true;
                    server->queue(moved_event);

                    moved_event.ids_.clear();
                }

                moved_event.arg2_ = old_parent;
                moved_event.ids_.push_back(ids[i]);

                sms_prog->msg_done_++;
            }
        }

        if (!flushed) {
            server->queue(moved_event);
        }

        if (sms_prog->msg_done_ == sms_prog->msg_count_) {
            sms_prog->state_ = epoc::msv::send_state_done;
        }

        state(epoc::msv::operation_state_success);
        complete_info_.complete(epoc::error_none);
    }

    std::int32_t schedule_copy_operation::system_progress(epoc::msv::system_progress_info &progress) {
        progress = sys_progress_;
        return epoc::error_none;
    }
}