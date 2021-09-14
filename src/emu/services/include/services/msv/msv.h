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
#include <services/msv/factory.h>
#include <services/msv/registry.h>
#include <services/msv/operations/base.h>

#include <kernel/server.h>
#include <services/framework.h>

#include <utils/des.h>
#include <utils/reqsts.h>

#include <memory>
#include <queue>

namespace eka2l1 {
    class io_system;
    class fs_server;
    struct fs_server_client;

    enum msv_opcode {
        msv_get_entry = 0x0,
        msv_get_children = 0x1,
        msv_change_entry = 0x6,
        msv_create_entry = 0x7,
        msv_move_entries = 0x9,
        msv_notify_session_event = 0xB,
        msv_cancel_notify_session_event = 0xC,
        msv_read_store = 0xD,
        msv_lock_store = 0xE,
        msv_release_store = 0xF,
        msv_operation_data = 0x12,
        msv_command_data = 0x13,
        msv_operation_progress = 0x15,
        msv_operation_completion = 0x16,
        msv_operation_mtm = 0x17,
        msv_mtm_command = 0x18,
        msv_mtm_group_ref = 0x1C,
        msv_mtm_group_unref = 0x1D,
        msv_fill_registered_mtm_dll_array = 0x19,
        msv_get_message_directory = 0x25,
        msv_will_you_take_more_work = 0x26,
        msv_set_as_observer_only = 0x27,
        msv_get_notify_sequence = 0x2D,
        msv_get_mtm_path = 0x2E,
        msv_set_mtm_path = 0x2F,
        msv_set_receive_entry_events = 0x31,
        msv_dec_store_reader_count = 0x32,
        msv_get_message_drive = 0x33,
        msv_get_required_capabilities = 0x34,
        msv_open_file_store_for_read = 0x39,
        msv_open_temp_store = 0x3A,
        msv_replace_file_store = 0x3B,
        msv_file_store_exist = 0x3D,
        msv_system_progress = 0x47
    };

    void absorb_command_data(common::chunkyseri &seri, std::vector<epoc::msv::msv_id> &ids, std::uint32_t &param1,
        std::uint32_t &param2);

    struct msv_event_data {
        epoc::msv::change_notification_type nof_;
        std::uint32_t arg1_;
        std::uint32_t arg2_;
        std::vector<std::uint32_t> ids_;
    };

    // To compatible with all versions :)
    static constexpr std::uint32_t MAX_ID_PER_EVENT_DATA_REPORT = 15;

    class msv_server : public service::typical_server {
        friend struct msv_client_session;

        std::u16string message_folder_;
        epoc::msv::mtm_registry reg_;

        std::unique_ptr<epoc::msv::entry_indexer> indexer_;
        std::vector<std::unique_ptr<epoc::msv::operation_factory>> factories_;

        fs_server *fserver_;

        bool inited_;

    protected:
        void install_rom_mtm_modules();
        void init();
        void init_sms_settings();

    public:
        explicit msv_server(eka2l1::system *sys);

        const std::u16string message_folder() const {
            return message_folder_;
        }
        
        epoc::msv::entry_indexer *indexer() {
            return indexer_.get();
        }

        io_system *get_io_system();

        void connect(service::ipc_context &context) override;
        void queue(msv_event_data &evt);

        bool move_entry(const std::uint32_t id, const std::uint32_t new_parent_id);

        void install_factory(std::unique_ptr<epoc::msv::operation_factory> &factory);
        epoc::msv::operation_factory *get_factory(const std::uint32_t mtm_uid);

        void absorb_entry_to_buffer(common::chunkyseri &seri, epoc::msv::entry &ent);
        void absorb_entry_and_owning_service_id_to_buffer(common::chunkyseri &seri, epoc::msv::entry &ent, std::uint32_t &owning_service);
    };

    struct msv_client_session : public service::typical_session {
        epoc::notify_info msv_info_;
        epoc::des8 *change_;
        epoc::des8 *sequence_;

        std::queue<msv_event_data> events_;
        std::uint32_t nof_sequence_;

        enum msv_session_flag {
            FLAG_RECEIVE_ENTRY_EVENTS = 1 << 0,
            FLAG_OBSERVER_ONLY = 1 << 1
        };

        std::uint32_t flags_;

        std::vector<epoc::msv::entry *> child_entries_;

        std::map<std::uint32_t, epoc::msv::operation_buffer> operation_buffers_;
        std::vector<std::shared_ptr<epoc::msv::operation>> operations_;

    protected:
        bool listen(epoc::notify_info &info, epoc::des8 *change, epoc::des8 *seq);
        bool claim_operation_buffer(const std::uint32_t operation_id, epoc::msv::operation_buffer &buffer);
        fs_server_client *make_new_fs_client(service::session *&ss);

    public:
        explicit msv_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

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
        void get_mtm_path(service::ipc_context *ctx);
        void set_mtm_path(service::ipc_context *ctx);
        void read_store(service::ipc_context *ctx);
        void lock_store(service::ipc_context *ctx);
        void release_store(service::ipc_context *ctx);
        void dec_store_read_count(service::ipc_context *ctx);
        void will_you_take_more_work(service::ipc_context *ctx);
        void operation_data(service::ipc_context *ctx);
        void create_entry(service::ipc_context *ctx);
        void change_entry(service::ipc_context *ctx);
        void move_entries(service::ipc_context *ctx);
        void command_data(service::ipc_context *ctx);
        void operation_info(service::ipc_context *ctx, const bool is_complete, const bool is_system = false);
        void transfer_mtm_command(service::ipc_context *ctx);
        void open_file_store_for_read(service::ipc_context *ctx);
        void open_temp_file_store(service::ipc_context *ctx);
        void replace_file_store(service::ipc_context *ctx);
        void file_store_exists(service::ipc_context *ctx);
        void required_capabilities(service::ipc_context *ctx);
        void operation_mtm(service::ipc_context *ctx);

        // Do not add reference here!
        void queue(msv_event_data evt);
    };
}
