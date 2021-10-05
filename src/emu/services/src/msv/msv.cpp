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

#include <services/msv/msv.h>
#include <services/msv/store.h>
#include <services/msv/operations/change.h>
#include <services/msv/operations/create.h>
#include <services/msv/operations/move.h>
#include <services/sms/mtm/factory.h>
#include <services/sms/settings.h>
#include <services/fs/fs.h>
#include <services/fs/std.h>

#include <system/epoc.h>
#include <vfs/vfs.h>

#include <utils/consts.h>
#include <utils/err.h>
#include <vfs/vfs.h>

#include <utils/bafl.h>
#include <utils/sec.h>

#include <common/cvt.h>
#include <common/time.h>

#include <system/devices.h>

#include <common/path.h>

namespace eka2l1 {
    static const std::u16string DEFAULT_MSG_DATA_DIR = u"C:\\private\\1000484b\\Mail2\\";
    static const std::u16string DEFAULT_MSG_DATA_DIR_OLD = u"C:\\System\\Mail\\";

    std::string get_msv_server_name_by_epocver(const epocver ver) {
        if (ver < epocver::epoc81a) {
            return "MsvServer";
        }

        return "!MsvServer";
    }

    msv_server::msv_server(eka2l1::system *sys)
        : service::typical_server(sys, get_msv_server_name_by_epocver((sys->get_symbian_version_use())))
        , reg_(sys->get_io_system())
        , fserver_(nullptr)
        , inited_(false) {
    }

    void msv_server::install_rom_mtm_modules() {
        io_system *io = sys->get_io_system();
        std::u16string DEFAULT_MSG_MTM_FILE_DIR;
        if (sys->get_symbian_version_use() <= epocver::eka2) {
            DEFAULT_MSG_MTM_FILE_DIR = u"!:\\System\\mtm\\*.r*";
        } else {
            DEFAULT_MSG_MTM_FILE_DIR = u"!:\\Resource\\Messaging\\Mtm\\*.r*";
        }

        drive_number drv_target = drive_z;

        for (drive_number drv = drive_z; drv >= drive_a; drv--) {
            auto drv_entry = io->get_drive_entry(drv);

            if (drv_entry && (drv_entry->media_type == drive_media::rom)) {
                drv_target = drv;
                break;
            }
        }

        DEFAULT_MSG_MTM_FILE_DIR[0] = drive_to_char16(drv_target);

        auto mtm_module_dir = io->open_dir(DEFAULT_MSG_MTM_FILE_DIR, {}, io_attrib_include_file);

        if (mtm_module_dir) {
            while (std::optional<entry_info> info = mtm_module_dir->get_next_entry()) {
                const std::u16string nearest = utils::get_nearest_lang_file(io,
                    common::utf8_to_ucs2(info->full_path), sys->get_system_language(), drv_target);

                reg_.install_group(nearest);
            }
        }
    }
    
    io_system *msv_server::get_io_system() {
        return sys->get_io_system();
    }

    static constexpr std::uint32_t SMS_SETTINGS_STORE_UID = 0x1000996E;

    void msv_server::init_sms_settings() {
        epoc::sms::sms_settings settings;
        epoc::sms::supply_sim_settings(sys, &settings);

        std::vector<epoc::msv::entry*> entries = indexer_->get_entries_by_parent(epoc::msv::MSV_ROOT_ID_VALUE);
        for (std::size_t i = 0; i < entries.size(); i++) {
            if (entries[i]->mtm_uid_ == epoc::msv::MSV_MSG_TYPE_UID) {
                std::optional<std::u16string> path = indexer_->get_entry_data_file(entries[i]);
                if (path.has_value()) {
                    io_system *io = sys->get_io_system();
                    symfile f = io->open_file(path.value(), READ_MODE | BIN_MODE);

                    epoc::msv::store the_store;

                    if (f) {
                        eka2l1::ro_file_stream rstream(f.get());
                        the_store.read(rstream);
                    }

                    epoc::msv::store_buffer &buffer = the_store.buffer_for(SMS_SETTINGS_STORE_UID);
                    common::chunkyseri seri(nullptr, 0, common::SERI_MODE_MEASURE);

                    settings.absorb(seri);

                    buffer.resize(seri.size());
                    seri = common::chunkyseri(buffer.data(), buffer.size(), common::SERI_MODE_WRITE);
                    settings.absorb(seri);

                    if (f) {
                        f->close();
                    }

                    f = io->open_file(path.value(), WRITE_MODE | BIN_MODE);
                    
                    if (f) {
                        eka2l1::wo_file_stream wstream(f.get());
                        the_store.write(wstream);

                        f->close();
                    }
                }
            }
        }
    }

    void msv_server::init() {
        // Instantiate the message folder
        device_manager *mngr = sys->get_device_manager();
        message_folder_ = eka2l1::add_path(kern->is_eka1() ? DEFAULT_MSG_DATA_DIR_OLD : DEFAULT_MSG_DATA_DIR,
            common::utf8_to_ucs2(mngr->get_current()->firmware_code) + u"\\");

        io_system *io = sys->get_io_system();

        if (!io->exist(message_folder_)) {
            io->create_directories(message_folder_);
        }

        if (kern->is_eka1()) {
            reg_.set_list_path(u"C:\\System\\mtm\\" + common::utf8_to_ucs2(mngr->get_current()->firmware_code)
                + u"\\Mtm Registry v2");
        }

        reg_.load_mtm_list();
        install_rom_mtm_modules();

        indexer_ = std::make_unique<epoc::msv::sql_entry_indexer>(io, message_folder_, sys->get_system_language());

        std::unique_ptr<epoc::msv::operation_factory> sms_opftr = std::make_unique<epoc::sms::sms_operation_factory>();
        install_factory(sms_opftr);

        fserver_ = reinterpret_cast<fs_server*>(kern->get_by_name<service::server>(eka2l1::epoc::fs::get_server_name_through_epocver(
            kern->get_epoc_version())));

        init_sms_settings();
        inited_ = true;
    }

    void msv_server::connect(service::ipc_context &context) {
        if (!inited_) {
            init();
        }

        msv_client_session *sess = create_session<msv_client_session>(&context);

        // Queue a ready event
        msv_event_data ready;
        ready.arg1_ = 0;
        ready.arg2_ = 0;
        ready.nof_ = epoc::msv::change_notification_type_index_loaded;

        sess->queue(ready);
        context.complete(epoc::error_none);
    }

    void msv_server::queue(msv_event_data &evt) {
        // Split to multiple event in case too much
        std::vector<msv_event_data> reports;

        if (evt.ids_.size() > MAX_ID_PER_EVENT_DATA_REPORT) {
            for (std::size_t i = 0; i < (evt.ids_.size() + MAX_ID_PER_EVENT_DATA_REPORT - 1) / MAX_ID_PER_EVENT_DATA_REPORT; i++) {
                msv_event_data copy;
                copy.nof_ = evt.nof_;
                copy.arg1_ = evt.arg1_;
                copy.arg2_ = evt.arg2_;

                copy.ids_.insert(copy.ids_.begin(), evt.ids_.begin() + i * MAX_ID_PER_EVENT_DATA_REPORT, (evt.ids_.size() < (i + 1) * MAX_ID_PER_EVENT_DATA_REPORT)
                    ? evt.ids_.end() : evt.ids_.begin() + (i + 1) * MAX_ID_PER_EVENT_DATA_REPORT);

                reports.push_back(copy);
            }
        }

        for (auto &session: sessions) {
            msv_client_session *cli = reinterpret_cast<msv_client_session*>(session.second.get());
            
            if (!reports.empty()) {
                for (std::size_t i = 0; i < reports.size(); i++) {
                    cli->queue(reports[i]);
                }
            } else {
                cli->queue(evt);
            }
        }
    }

    // When using move to change parent, you are guranteed that your store is also gonna be moved
    bool msv_server::move_entry(const std::uint32_t id, const std::uint32_t new_parent_id) {
        std::uint32_t current_own_service = 0;
        std::uint32_t new_own_service = 0;

        if (!indexer_->owning_service(id, current_own_service)) {
            LOG_ERROR(SERVICE_MSV, "Unable to get current owning service of entry id 0x{:X}", id);
            return false;
        }

        if (!indexer_->owning_service(new_parent_id, new_own_service)) {
            LOG_ERROR(SERVICE_MSV, "Unable to get current owning service of entry id 0x{:X}", id);
            return false;
        }

        if (!indexer_->move_entry(id, new_parent_id)) {
            return false;
        }

        if (current_own_service != new_own_service) {
            // Move attachments to new folder
            std::optional<std::u16string> old_folder_path = indexer_->get_entry_data_directory(id,
                epoc::msv::MSV_NORMAL_UID, current_own_service);

            std::optional<std::u16string> new_folder_path = indexer_->get_entry_data_directory(id,
                epoc::msv::MSV_NORMAL_UID, new_own_service);

            if (!old_folder_path.has_value() || !new_folder_path.has_value()) {
                LOG_WARN(SERVICE_MSV, "Unable to construct store path, store will not be moved");
            } else {
                const std::u16string store_name = epoc::msv::get_folder_name(id, epoc::msv::MSV_FOLDER_TYPE_NORMAL);
                const std::u16string external_name = epoc::msv::get_folder_name(id, epoc::msv::MSV_FOLDER_TYPE_PATH);

                io_system *io = sys->get_io_system();

                std::u16string path_to_store = eka2l1::add_path(old_folder_path.value(), store_name);
                if (io->exist(path_to_store)) {
                    io->rename(path_to_store, eka2l1::add_path(new_folder_path.value(), store_name));
                }

                std::u16string path_to_external = eka2l1::add_path(old_folder_path.value(), external_name);
                if (io->exist(path_to_external)) {
                    io->rename(path_to_external, eka2l1::add_path(new_folder_path.value(), external_name));
                }
            }
        }

        return true;
    }

    void msv_server::install_factory(std::unique_ptr<epoc::msv::operation_factory> &factory) {
        factories_.push_back(std::move(factory));
    }
    
    epoc::msv::operation_factory *msv_server::get_factory(const std::uint32_t mtm_uid) {
        for (std::size_t i = 0; i < factories_.size(); i++) {
            if (factories_[i]->mtm_uid() == mtm_uid) {
                return factories_[i].get();
            }
        }

        return nullptr;
    }

    static void pad_out_data(common::chunkyseri &seri) {
        std::uint8_t padding[3];

        // Pad out
        if (seri.size() % 4 != 0) {
            seri.absorb_impl(padding, 4 - seri.size() % 4);
        }
    }

    void msv_server::absorb_entry_to_buffer(common::chunkyseri &seri, epoc::msv::entry &ent) {
        epoc::msv::entry_data data_str;
        if (seri.get_seri_mode() == common::SERI_MODE_WRITE) {
            data_str.data_ = ent.data_;
            data_str.date_ = ent.time_;
            data_str.id_ = ent.id_;
            data_str.parent_id_ = ent.parent_id_;
            data_str.service_id_ = ent.service_id_;
            data_str.mtm_uid_ = ent.mtm_uid_;
            data_str.type_uid_ = ent.type_uid_;
            data_str.bio_type_ = ent.bio_type_;
            data_str.size_ = ent.size_;

            data_str.description_.set_length(nullptr, static_cast<std::uint32_t>(ent.description_.length()));
            data_str.details_.set_length(nullptr, static_cast<std::uint32_t>(ent.details_.length()));
        }
        
        if (kern->is_eka1()) {
            seri.absorb_impl(reinterpret_cast<std::uint8_t *>(&data_str), sizeof(epoc::msv::entry_data));
        } else {
            // The date value is aligned to offset 40, to fit 8-bit alignment
            // This is due to change of compiler compiling these built-in DLLs in EKA2
            seri.absorb_impl(reinterpret_cast<std::uint8_t*>(&data_str), offsetof(epoc::msv::entry_data, date_));

            std::uint32_t padding = 0;
            seri.absorb(padding);

            seri.absorb_impl(reinterpret_cast<std::uint8_t*>(&data_str.date_), sizeof(epoc::msv::entry_data) - offsetof(epoc::msv::entry_data, date_));
        }

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            ent.data_ = data_str.data_;
            ent.time_ = data_str.date_;
            ent.id_ = data_str.id_;
            ent.parent_id_ = data_str.parent_id_;
            ent.service_id_ = data_str.service_id_;
            ent.mtm_uid_ = data_str.mtm_uid_;
            ent.type_uid_ = data_str.type_uid_;
            ent.bio_type_ = data_str.bio_type_;
            ent.size_ = data_str.size_;

            ent.description_.resize(data_str.description_.get_length());
            ent.details_.resize(data_str.details_.get_length());
        }

        pad_out_data(seri);
        seri.absorb_impl(reinterpret_cast<std::uint8_t*>(ent.description_.data()), ent.description_.length() * sizeof(char16_t));

        pad_out_data(seri);
        seri.absorb_impl(reinterpret_cast<std::uint8_t*>(ent.details_.data()), ent.details_.length() * sizeof(char16_t));

        pad_out_data(seri);
    }

    void msv_server::absorb_entry_and_owning_service_id_to_buffer(common::chunkyseri &seri, epoc::msv::entry &ent, std::uint32_t &owning_service) {
        absorb_entry_to_buffer(seri, ent);
        seri.absorb(owning_service);
    }

    msv_client_session::msv_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version)
        , flags_(0)
        , nof_sequence_(0) {
    }

    static void pack_change_buffer(epoc::des8 *buf, kernel::process *pr, const msv_event_data &data) {
        std::uint32_t *buffer_raw = reinterpret_cast<std::uint32_t *>(buf->get_pointer_raw(pr));

        if (!buffer_raw) {
            LOG_ERROR(SERVICE_MSV, "Can't get change buffer raw pointer!");
            return;
        }

        const std::uint32_t total_int32 = static_cast<std::uint32_t>(4 + data.ids_.size());

        if (buf->get_max_length(pr) < total_int32 * sizeof(std::uint32_t)) {
            LOG_ERROR(SERVICE_MSV, "Insufficent change buffer size");
            return;
        }

        buffer_raw[0] = static_cast<std::uint32_t>(data.nof_);
        buffer_raw[1] = data.arg1_;
        buffer_raw[2] = data.arg2_;
        buffer_raw[3] = static_cast<std::uint32_t>(data.ids_.size());

        for (std::size_t i = 0; i < data.ids_.size(); i++) {
            buffer_raw[4 + i] = data.ids_[i];
        }

        buf->set_length(pr, total_int32 * sizeof(std::uint32_t));
    }

    bool msv_client_session::listen(epoc::notify_info &info, epoc::des8 *change, epoc::des8 *seq) {
        if (!msv_info_.empty()) {
            return false;
        }

        if (!events_.empty()) {
            msv_event_data evt = std::move(events_.front());
            events_.pop();

            kernel::process *own_pr = info.requester->owning_process();

            nof_sequence_++;
            std::string nof_sequence_data(reinterpret_cast<const char *>(&nof_sequence_), sizeof(std::uint32_t));

            // Copy change
            pack_change_buffer(change, own_pr, evt);
            seq->assign(own_pr, nof_sequence_data);

            info.complete(epoc::error_none);
            return true;
        }

        msv_info_ = info;
        change_ = change;
        sequence_ = seq;

        return true;
    }

    void msv_client_session::queue(msv_event_data evt) {
        nof_sequence_++;
        std::string nof_sequence_data(reinterpret_cast<const char *>(&nof_sequence_), sizeof(std::uint32_t));

        if (!msv_info_.empty()) {
            kernel::process *own_pr = msv_info_.requester->owning_process();

            // Copy change
            pack_change_buffer(change_, own_pr, evt);
            sequence_->assign(own_pr, nof_sequence_data);

            msv_info_.complete(epoc::error_none);
            return;
        }

        events_.push(std::move(evt));
    }

    void msv_client_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case msv_get_entry:
            get_entry(ctx);
            break;

        case msv_get_children:
            get_children(ctx);
            break;

        case msv_notify_session_event: {
            notify_session_event(ctx);
            break;
        }

        case msv_cancel_notify_session_event:
            cancel_notify_session_event(ctx);
            break;

        case msv_mtm_group_ref:
            ref_mtm_group(ctx);
            break;

        case msv_mtm_group_unref:
            unref_mtm_group(ctx);
            break;

        case msv_fill_registered_mtm_dll_array:
            fill_registered_mtm_dll_array(ctx);
            break;

        case msv_get_mtm_path:
            get_mtm_path(ctx);
            break;

        case msv_set_mtm_path:
            set_mtm_path(ctx);
            break;

        case msv_get_message_directory:
            get_message_directory(ctx);
            break;

        case msv_set_as_observer_only:
            set_as_observer_only(ctx);
            break;

        case msv_get_notify_sequence:
            get_notify_sequence(ctx);
            break;

        case msv_set_receive_entry_events:
            set_receive_entry_events(ctx);
            break;

        case msv_get_message_drive:
            get_message_drive(ctx);
            break;

        case msv_read_store:
            read_store(ctx);
            break;

        case msv_lock_store:
            lock_store(ctx);
            break;

        case msv_release_store:
            release_store(ctx);
            break;

        case msv_dec_store_reader_count:
            dec_store_read_count(ctx);
            break;

        case msv_will_you_take_more_work:
            will_you_take_more_work(ctx);
            break;

        case msv_operation_data:
            operation_data(ctx);
            break;

        case msv_command_data:
            command_data(ctx);
            break;

        case msv_change_entry:
            change_entry(ctx);
            break;

        case msv_create_entry:
            create_entry(ctx);
            break;

        case msv_move_entries:
            move_entries(ctx);
            break;

        case msv_operation_progress:
            operation_info(ctx, false);
            break;

        case msv_operation_completion:
            operation_info(ctx, true);
            break;

        case msv_system_progress:
            operation_info(ctx, false, true);
            break;

        case msv_mtm_command:
            transfer_mtm_command(ctx);
            break;

        case msv_open_file_store_for_read:
            open_file_store_for_read(ctx);
            break;

        case msv_open_temp_store:
            open_temp_file_store(ctx);
            break;

        case msv_replace_file_store:
            replace_file_store(ctx);
            break;

        case msv_get_required_capabilities:
            required_capabilities(ctx);
            break;

        case msv_file_store_exist:
            file_store_exists(ctx);
            break;

        case msv_operation_mtm:
            operation_mtm(ctx);
            break;

        case msv_cancel_operation:
            cancel_operation(ctx);
            break;

        default: {
            LOG_ERROR(SERVICE_MSV, "Unimplemented opcode for MsvServer 0x{:X}", ctx->msg->function);
            break;
        }
        }
    }

    void msv_client_session::notify_session_event(service::ipc_context *ctx) {
        kernel::process *own_pr = ctx->msg->own_thr->owning_process();
        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);

        // Remember
        epoc::des8 *change = eka2l1::ptr<epoc::des8>(ctx->msg->args.args[0]).get(own_pr);
        epoc::des8 *select = eka2l1::ptr<epoc::des8>(ctx->msg->args.args[1]).get(own_pr);

        if (!listen(info, change, select)) {
            LOG_ERROR(SERVICE_MSV, "Request listen on an already-listening MSV session");
            ctx->complete(epoc::error_in_use);
        }
    }

    void msv_client_session::cancel_notify_session_event(service::ipc_context *ctx) {
        if (!msv_info_.empty()) {
            msv_info_.complete(epoc::error_cancel);
        }

        ctx->complete(epoc::error_none);
    }

    void msv_client_session::get_message_directory(service::ipc_context *ctx) {
        const std::u16string folder = server<msv_server>()->message_folder();

        ctx->write_arg(0, folder);
        ctx->complete(epoc::error_none);
    }

    void msv_client_session::get_message_drive(service::ipc_context *ctx) {
        const std::u16string folder = server<msv_server>()->message_folder();
        const drive_number drv = char16_to_drive(folder[0]);

        ctx->complete(static_cast<int>(drv));
    }

    void msv_client_session::set_receive_entry_events(service::ipc_context *ctx) {
        std::optional<std::int32_t> receive = ctx->get_argument_value<std::int32_t>(0);

        if (!receive) {
            ctx->complete(epoc::error_argument);
            return;
        }

        flags_ &= ~FLAG_RECEIVE_ENTRY_EVENTS;

        if (receive.value()) {
            flags_ |= FLAG_RECEIVE_ENTRY_EVENTS;
        }

        ctx->complete(epoc::error_none);
    }
    
    void absorb_command_data(common::chunkyseri &seri, std::vector<epoc::msv::msv_id> &ids, std::uint32_t &param1,
        std::uint32_t &param2) {
        std::uint32_t count = static_cast<std::uint32_t>(ids.size());
        seri.absorb(count);

        if (seri.get_seri_mode() == common::SERI_MODE_READ)
            ids.resize(count);

        for (std::size_t i = 0; i < ids.size(); i++) {
            seri.absorb(ids[i]);
        }

        seri.absorb(param1);
        seri.absorb(param2);
    }

    void msv_client_session::get_entry(service::ipc_context *ctx) {
        epoc::msv::entry_indexer *indexer = server<msv_server>()->indexer_.get();
        std::optional<epoc::uid> id = ctx->get_argument_value<epoc::uid>(0);

        if (!id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::msv::entry *ent = indexer->get_entry(id.value());
        if (!ent) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        // Try to serialize this buffer
        std::uint8_t *buffer = reinterpret_cast<std::uint8_t *>(ctx->get_descriptor_argument_ptr(1));
        std::size_t buffer_max_size = ctx->get_argument_max_data_size(1);

        if (!buffer || !buffer_max_size) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::uint32_t owning_service_id = 0;
        if (!indexer->owning_service(ent->id_, owning_service_id)) {
            LOG_WARN(SERVICE_MSV, "Unable to obtain owning service ID!");
        }

        msv_server *serv = server<msv_server>();

        common::chunkyseri seri(buffer, buffer_max_size, common::SERI_MODE_MEASURE);
        serv->absorb_entry_and_owning_service_id_to_buffer(seri, *ent, owning_service_id);

        if (seri.size() > buffer_max_size) {
            ctx->complete(epoc::error_overflow);
            return;
        }

        seri = common::chunkyseri(buffer, buffer_max_size, common::SERI_MODE_WRITE);
        serv->absorb_entry_and_owning_service_id_to_buffer(seri, *ent, owning_service_id);

        ctx->set_descriptor_argument_length(1, static_cast<std::uint32_t>(seri.size()));
        
        // Slot 2 is usually service ID on old MSV
        if (ctx->get_descriptor_argument_ptr(2)) {
            ctx->write_data_to_descriptor_argument<std::uint32_t>(2, owning_service_id);
        }

        ctx->complete(epoc::error_none);
    }

    struct children_details {
        std::uint32_t parent_id_;
        std::uint32_t child_count_total_;
        std::uint32_t child_count_in_array_;
        std::uint32_t last_entry_in_array_;
    };

    void msv_client_session::get_children(service::ipc_context *ctx) {
        std::optional<children_details> details = ctx->get_argument_data_from_descriptor<children_details>(0);

        if (!details) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::msv::entry_indexer *indexer = server<msv_server>()->indexer_.get();
        child_entries_ = indexer->get_entries_by_parent(details->parent_id_);

        // TODO(pent0): Include the selection flags in slot 1
        std::uint8_t *buffer = reinterpret_cast<std::uint8_t *>(ctx->get_descriptor_argument_ptr(2));
        std::size_t buffer_max_size = ctx->get_argument_max_data_size(2);

        if (!buffer || !buffer_max_size) {
            ctx->complete(epoc::error_argument);
            return;
        }

        msv_server *serv = server<msv_server>();

        details->child_count_total_ = static_cast<std::uint32_t>(child_entries_.size());
        details->child_count_in_array_ = 0;

        std::size_t written = 0;
        common::chunkyseri seri(buffer, buffer_max_size, common::SERI_MODE_WRITE);

        bool is_overflow = false;

        while ((written < buffer_max_size) && (!child_entries_.empty())) {
            common::chunkyseri measurer(nullptr, 0x1000, common::SERI_MODE_MEASURE);
            serv->absorb_entry_to_buffer(measurer, *child_entries_.back());

            if (measurer.size() + written > buffer_max_size) {
                is_overflow = true;
                break;
            }

            // Start writing to this buffer
            serv->absorb_entry_to_buffer(seri, *child_entries_.back());

            details->child_count_in_array_++;

            // TODO: Not sure.
            details->last_entry_in_array_ = details->child_count_in_array_;

            child_entries_.pop_back();

            written = seri.size();
        }

        ctx->write_data_to_descriptor_argument(0, details.value());
        ctx->set_descriptor_argument_length(2, static_cast<std::uint32_t>(seri.size()));

        if (is_overflow) {
            ctx->complete(epoc::error_overflow);
        } else {
            ctx->complete(epoc::error_none);
        }
    }

    void msv_client_session::get_notify_sequence(service::ipc_context *ctx) {
        ctx->write_data_to_descriptor_argument<std::uint32_t>(0, nof_sequence_);
        ctx->complete(epoc::error_none);
    }

    void msv_client_session::ref_mtm_group(service::ipc_context *ctx) {
        std::optional<epoc::uid> uid = ctx->get_argument_value<epoc::uid>(0);

        if (!uid.value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::msv::mtm_group *group = server<msv_server>()->reg_.query_mtm_group(uid.value());

        if (!group) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        group->ref_count_++;
        ctx->complete(epoc::error_none);
    }

    void msv_client_session::unref_mtm_group(service::ipc_context *ctx) {
        std::optional<epoc::uid> uid = ctx->get_argument_value<epoc::uid>(0);

        if (!uid.value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::msv::mtm_group *group = server<msv_server>()->reg_.query_mtm_group(uid.value());

        if (!group) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        if (group->ref_count_ == 0) {
            ctx->complete(epoc::error_underflow);
            return;
        }

        group->ref_count_--;
        ctx->complete(epoc::error_none);
    }

    void msv_client_session::set_as_observer_only(service::ipc_context *ctx) {
        flags_ |= FLAG_OBSERVER_ONLY;
        ctx->complete(epoc::error_none);
    }

    void msv_client_session::fill_registered_mtm_dll_array(service::ipc_context *ctx) {
        std::optional<std::uint32_t> mtm_uid = ctx->get_argument_value<std::uint32_t>(0);

        if (!mtm_uid) {
            ctx->complete(epoc::error_argument);
            return;
        }

        auto comps = server<msv_server>()->reg_.get_components(mtm_uid.value());
        kernel_system *kern = server<msv_server>()->get_kernel_object_owner();

        std::uint8_t *argptr = ctx->get_descriptor_argument_ptr(1);
        const std::size_t argsize = ctx->get_argument_max_data_size(1);

        // Reserves 4 bytes for count
        common::chunkyseri seri(argptr + 4, argsize - 4, common::SERI_MODE_WRITE);

        std::uint32_t *element_count = reinterpret_cast<std::uint32_t *>(argptr);
        *element_count = 0;

        for (auto &comp : comps) {
            // Make a way for break in future
            (*element_count)++;
            epoc::msv::mtm_group *group = server<msv_server>()->reg_.get_group(comp->group_idx_);

            seri.absorb(group->mtm_uid_);
            seri.absorb(group->tech_type_uid_);

            // WARN (pent0): Currently force it to eat all stuffs as UTF8
            // std::string name_utf8 = common::ucs2_to_utf8(comp->name_);
            if (kern->is_eka1()) {
                std::uint32_t length = static_cast<std::uint32_t>(comp->name_.length()) | (epoc::buf << 28);
                std::uint32_t max_length = 50;

                seri.absorb(length);
                seri.absorb(max_length);

                seri.absorb_impl(reinterpret_cast<std::uint8_t *>(comp->name_.data()), comp->name_.length() * sizeof(char16_t));

                // This was supposed to be a TBuf<50>
                std::uint32_t total_bytes_reserved = 100;
                seri.absorb_impl(nullptr, total_bytes_reserved - comp->name_.length() * sizeof(char16_t));
            } else {
                epoc::absorb_des_string(comp->name_, seri, true);

                seri.absorb(group->cap_send_);
                seri.absorb(group->cap_body_);
                seri.absorb(group->cap_avail_);
            }

            std::uint32_t dll_uid = 0x10000079; ///< Dynamic DLL UID
            seri.absorb(dll_uid);
            seri.absorb(comp->comp_uid_);
            seri.absorb(comp->specific_uid_);

            std::uint32_t entry_point = comp->entry_point_;
            seri.absorb(entry_point);

            // Absorb versions
            std::uint32_t version = (comp->build_) | ((comp->major_ & 0xFF) << 16) | ((comp->minor_ & 0xFF) << 8);
            seri.absorb(version);

            // Legacy does not contain filename
            if (!kern->is_eka1() && (comp->specific_uid_ == epoc::msv::MTM_DEFAULT_SPECIFIC_UID)) {
                //std::string filename_utf8 = common::ucs2_to_utf8(comp->filename_);
                epoc::absorb_des_string(comp->filename_, seri, true);
            }

            if (kern->is_eka1() && (kern->get_epoc_version() >= epocver::epoc81a)) {
                std::uint32_t cap_send_32 = group->cap_send_;
                std::uint32_t cap_body_32 = group->cap_body_;
                std::uint32_t cap_avail_32 = group->cap_avail_;

                seri.absorb(cap_send_32);
                seri.absorb(cap_body_32);
                seri.absorb(cap_avail_32);
            }
        }

        ctx->set_descriptor_argument_length(1, static_cast<std::uint32_t>(seri.size()) + 4);
        ctx->complete(epoc::error_none);
    }

    void msv_client_session::get_mtm_path(service::ipc_context *ctx) {
        std::optional<epoc::uid> uid1 = ctx->get_argument_value<epoc::uid>(0);
        std::optional<epoc::uid> uid2 = ctx->get_argument_value<epoc::uid>(1);
        std::optional<epoc::uid> uid3 = ctx->get_argument_value<epoc::uid>(2);

        if (!uid1.has_value() || !uid2.has_value() || !uid3.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::uid_type type_to_find;
        type_to_find.uid1 = uid1.value();
        type_to_find.uid2 = uid2.value();
        type_to_find.uid3 = uid3.value();

        epoc::msv::mtm_component *comp = server<msv_server>()->reg_.query_mtm_component(type_to_find);
        if (!comp) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        if (comp->filename_.empty()) {
            ctx->complete(epoc::error_not_supported);
            return;
        }

        ctx->write_arg(3, comp->filename_);
        ctx->complete(epoc::error_none);
    }

    void msv_client_session::set_mtm_path(service::ipc_context *ctx) {
        std::optional<epoc::uid> uid1 = ctx->get_argument_value<epoc::uid>(0);
        std::optional<epoc::uid> uid2 = ctx->get_argument_value<epoc::uid>(1);
        std::optional<epoc::uid> uid3 = ctx->get_argument_value<epoc::uid>(2);
        std::optional<std::u16string> path = ctx->get_argument_value<std::u16string>(3);

        if (!uid1.has_value() || !uid2.has_value() || !uid3.has_value() || !path.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::uid_type type_to_find;
        type_to_find.uid1 = uid1.value();
        type_to_find.uid2 = uid2.value();
        type_to_find.uid3 = uid3.value();

        epoc::msv::mtm_component *comp = server<msv_server>()->reg_.query_mtm_component(type_to_find);
        if (!comp) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        comp->filename_ = path.value();
        ctx->complete(epoc::error_none);
    }

    void msv_client_session::read_store(service::ipc_context *ctx) {
        std::optional<std::uint32_t> store_id = ctx->get_argument_value<std::uint32_t>(0);
        if (!store_id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::msv::entry_indexer *indexer = server<msv_server>()->indexer_.get();
        epoc::msv::entry *ent = indexer->get_entry(store_id.value());
        if (!ent) {
            LOG_ERROR(SERVICE_MSV, "Entry with ID 0x{:X} does not exist", store_id.value());
            ctx->complete(epoc::error_not_found);

            return;
        }

        if (ent->store_lock()) {
            ctx->complete(epoc::error_permission_denied);
            return;
        }

        ent->read_count_++;
        ctx->complete(epoc::error_none);
    }

    void msv_client_session::lock_store(service::ipc_context *ctx) {
        std::optional<std::uint32_t> store_id = ctx->get_argument_value<std::uint32_t>(0);
        if (!store_id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::msv::entry_indexer *indexer = server<msv_server>()->indexer_.get();
        epoc::msv::entry *ent = indexer->get_entry(store_id.value());
        if (!ent) {
            LOG_ERROR(SERVICE_MSV, "Entry with ID 0x{:X} does not exist", store_id.value());
            ctx->complete(epoc::error_not_found);

            return;
        }

        if (ent->store_lock()) {
            ctx->complete(epoc::error_locked);
            return;
        }

        ent->store_lock(true);
        ctx->complete(epoc::error_none);
    }

    void msv_client_session::release_store(service::ipc_context *ctx) {
        std::optional<std::uint32_t> store_id = ctx->get_argument_value<std::uint32_t>(0);
        if (!store_id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::msv::entry_indexer *indexer = server<msv_server>()->indexer_.get();
        epoc::msv::entry *ent = indexer->get_entry(store_id.value());
        if (!ent) {
            LOG_ERROR(SERVICE_MSV, "Entry with ID 0x{:X} does not exist", store_id.value());
            ctx->complete(epoc::error_not_found);

            return;
        }

        if (!ent->store_lock()) {
            LOG_ERROR(SERVICE_MSV, "Store ID 0x{:X} not locked to release", store_id.value());
            ctx->complete(epoc::error_general);
            return;
        }

        ent->store_lock(false);
        ctx->complete(epoc::error_none);
    }

    void msv_client_session::dec_store_read_count(service::ipc_context *ctx) {
        std::optional<std::uint32_t> store_id = ctx->get_argument_value<std::uint32_t>(0);
        if (!store_id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::msv::entry_indexer *indexer = server<msv_server>()->indexer_.get();
        epoc::msv::entry *ent = indexer->get_entry(store_id.value());
        if (!ent) {
            LOG_ERROR(SERVICE_MSV, "Entry with ID 0x{:X} does not exist", store_id.value());
            ctx->complete(epoc::error_not_found);

            return;
        }

        if (ent->read_count_ == 0) {
            LOG_ERROR(SERVICE_MSV, "Read count is already zero and can't be decreased more! (id=0x{:X})", store_id.value());
            ctx->complete(epoc::error_general);
            return;
        }

        ent->read_count_--;
        ctx->complete(epoc::error_none);
    }

    void msv_client_session::will_you_take_more_work(service::ipc_context *ctx) {
        ctx->complete(epoc::error_none);
    }

    void msv_client_session::operation_data(service::ipc_context *ctx) {
        std::optional<std::uint32_t> operation_id = ctx->get_argument_value<std::uint32_t>(0);
        std::uint8_t *operation_buffer_ptr = ctx->get_descriptor_argument_ptr(1);
        std::size_t operation_buffer_size = ctx->get_argument_data_size(1);

        if (!operation_id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        if (!operation_buffer_ptr || !operation_buffer_size) {
            LOG_WARN(SERVICE_MSV, "Operation data buffer is empty, clearing it out");

            auto ite = operation_buffers_.find(operation_id.value());
            if (ite != operation_buffers_.end()) {
                operation_buffers_.erase(ite);
            }
        } else {
            epoc::msv::operation_buffer &mee = operation_buffers_[operation_id.value()];
            mee.resize(operation_buffer_size);

            std::memcpy(mee.data(), operation_buffer_ptr, operation_buffer_size);
        }

        ctx->complete(epoc::error_none);
    }

    void msv_client_session::command_data(service::ipc_context *ctx) {
        std::optional<std::uint32_t> operation_id = ctx->get_argument_value<std::uint32_t>(0);

        std::uint8_t *operation_buffer_ptr = ctx->get_descriptor_argument_ptr(1);
        std::size_t operation_buffer_size = ctx->get_argument_data_size(1);

        std::uint8_t *parameter_ptr = ctx->get_descriptor_argument_ptr(2);
        std::size_t parameter_size = ctx->get_argument_data_size(2);

        if (!operation_id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        if (!operation_buffer_ptr || !operation_buffer_size) {
            LOG_WARN(SERVICE_MSV, "Operation data buffer is empty, clearing it out");

            auto ite = operation_buffers_.find(operation_id.value());
            if (ite != operation_buffers_.end()) {
                operation_buffers_.erase(ite);
            }
        } else {
            epoc::msv::operation_buffer &mee = operation_buffers_[operation_id.value()];
            mee.resize(operation_buffer_size);

            std::memcpy(mee.data(), operation_buffer_ptr, operation_buffer_size);

            if (parameter_ptr && parameter_size) {
                mee.insert(mee.begin(), parameter_ptr, parameter_ptr + parameter_size);
            }
        }

        ctx->complete(epoc::error_none);
    }

    bool msv_client_session::claim_operation_buffer(const std::uint32_t operation_id, epoc::msv::operation_buffer &buffer) {
        auto ite = operation_buffers_.find(operation_id);
        if (ite != operation_buffers_.end()) {
            buffer = std::move(ite->second);
            operation_buffers_.erase(ite);

            return true;
        }

        return false;
    }

    void msv_client_session::create_entry(service::ipc_context *ctx) {
        std::optional<std::uint32_t> operation_id = ctx->get_argument_value<std::uint32_t>(0);
        std::optional<std::uint32_t> process_id = ctx->get_argument_value<std::uint32_t>(1);

        // TODO: Consider the second argument in newer version for security.
        if (!operation_id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::msv::operation_buffer buffer;
        if (!claim_operation_buffer(operation_id.value(), buffer)) {
            LOG_ERROR(SERVICE_MSV, "No buffer correspond with operation ID 0x{:X} to create entry", operation_id.value());
            ctx->complete(epoc::error_argument);

            return;
        }

        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);

        std::shared_ptr<epoc::msv::operation> operation = std::make_shared<epoc::msv::create_operation>(
            operation_id.value(), buffer, info);

        operations_.push_back(operation);
        operation->execute(server<msv_server>(), process_id.value());
    }

    void msv_client_session::change_entry(service::ipc_context *ctx) {
        std::optional<std::uint32_t> operation_id = ctx->get_argument_value<std::uint32_t>(0);
        std::optional<std::uint32_t> process_id = ctx->get_argument_value<std::uint32_t>(1);

        // TODO: Consider the second argument in newer version for security.
        if (!operation_id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::msv::operation_buffer buffer;
        if (!claim_operation_buffer(operation_id.value(), buffer)) {
            LOG_ERROR(SERVICE_MSV, "No buffer correspond with operation ID 0x{:X} to create entry", operation_id.value());
            ctx->complete(epoc::error_argument);

            return;
        }

        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);

        std::shared_ptr<epoc::msv::operation> operation = std::make_shared<epoc::msv::change_operation>(
            operation_id.value(), buffer, info);

        operations_.push_back(operation);
        operation->execute(server<msv_server>(), process_id.value());
    }

    void msv_client_session::move_entries(service::ipc_context *ctx) {
        std::optional<std::uint32_t> operation_id = ctx->get_argument_value<std::uint32_t>(0);

        epoc::msv::operation_buffer buffer;
        if (!claim_operation_buffer(operation_id.value(), buffer)) {
            LOG_ERROR(SERVICE_MSV, "No buffer correspond with operation ID 0x{:X} to create entry", operation_id.value());
            ctx->complete(epoc::error_argument);

            return;
        }

        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);

        std::shared_ptr<epoc::msv::operation> operation = std::make_shared<epoc::msv::move_operation>(
            operation_id.value(), buffer, info);

        operations_.push_back(operation);
        operation->execute(server<msv_server>(), 0);
    }

    void msv_client_session::operation_info(service::ipc_context *ctx, const bool is_complete, const bool is_system) {
        std::optional<std::uint32_t> operation_id = ctx->get_argument_value<std::uint32_t>(0);
        
        if (!operation_id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        for (std::size_t i = 0; i < operations_.size(); i++) {
            if (operations_[i]->operation_id() == operation_id.value()) {
                if (operations_[i]->state() == epoc::msv::operation_state_queued) {
                    ctx->complete(epoc::error_not_ready);
                    return;
                }

                if (operations_[i]->state() == epoc::msv::operation_state_failure) {
                    ctx->complete(epoc::error_none);
                    return;
                }

                if (is_system) {
                    epoc::msv::system_progress_info progress;
                    const std::int32_t code = operations_[i]->system_progress(progress);

                    ctx->write_data_to_descriptor_argument<epoc::msv::system_progress_info>(1, progress);
                    ctx->complete(code);

                    return;
                } else {
                    if ((is_complete && (operations_[i]->state() == epoc::msv::operation_state_success)) ||
                        (!is_complete && (operations_[i]->state() == epoc::msv::operation_state_pending))) {
                        const epoc::msv::operation_buffer &buffer = operations_[i]->progress_buffer();

                        ctx->write_data_to_descriptor_argument(1, buffer.data(), static_cast<std::uint32_t>(buffer.size()));
                        ctx->complete(static_cast<int>(buffer.size()));

                        if (is_complete)
                            operations_.erase(operations_.begin() + i);

                        return;
                    }
                }

                LOG_ERROR(SERVICE_MSV, "Accessing operation 0x{:X} progress with contradict state={}!", operation_id.value(),
                    static_cast<int>(operations_[i]->state()));

                ctx->complete(epoc::error_argument);

                return;
            }
        }

        LOG_ERROR(SERVICE_MSV, "Progress buffer for operation 0x{:X} can't be found!", operation_id.value());
        ctx->complete(epoc::error_not_found);
    }

    void msv_client_session::transfer_mtm_command(service::ipc_context *ctx) {
        std::optional<std::uint32_t> operation_id = ctx->get_argument_value<std::uint32_t>(0);

        // As reviewed in source code, the first entry will indicates which MTM DLL/ factory will be used
        // to instatiate operation resoltuion
        if (!operation_id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        // Claim operation buffer
        epoc::msv::operation_buffer buffer;
        if (!claim_operation_buffer(operation_id.value(), buffer)) {
            LOG_ERROR(SERVICE_MSV, "No buffer correspond with operation ID 0x{:X} to create entry", operation_id.value());
            ctx->complete(epoc::error_argument);

            return;
        }

        std::vector<std::uint32_t> ids;
        std::uint32_t param1 = 0;
        std::uint32_t param2 = 0;

        common::chunkyseri seri(buffer.data(), buffer.size(), common::SERI_MODE_READ);
        absorb_command_data(seri, ids, param1, param2);

        if (ids.empty()) {
            LOG_ERROR(SERVICE_MSV, "Operating with empty list of entries!");
            ctx->complete(epoc::error_argument);

            return;
        }

        std::uint32_t mtm_id = 0;

        msv_server *serv = server<msv_server>();
        epoc::msv::entry *ent = serv->indexer()->get_entry(ids[0]);

        if (!ent) {
            LOG_ERROR(SERVICE_MSV, "First entry to validate command transfer does not exist!");
            ctx->complete(epoc::error_argument);

            return;
        }

        if (ent) {
            mtm_id = ent->mtm_uid_;
        }

        epoc::msv::operation_factory *factory = serv->get_factory(mtm_id);
        if (!factory) {
            LOG_ERROR(SERVICE_MSV, "No factory available with MTM UID 0x{:X}", mtm_id);
            ctx->complete(epoc::error_not_supported);

            return;
        }

        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);

        std::shared_ptr<epoc::msv::operation> operation = factory->create_operation(operation_id.value(),
            buffer, info, param1);

        if (!operation) {
            LOG_ERROR(SERVICE_MSV, "Operation with id 0x{:X} not supported with factory MTM UID 0x{:X}", param1, mtm_id);
            ctx->complete(epoc::error_not_supported);

            return;
        }

        operations_.push_back(operation);
        operation->execute(server<msv_server>(), 0);
    }

    fs_server_client *msv_client_session::make_new_fs_client(service::session *&ss) {
        msv_server *msver = server<msv_server>();
        kernel_system *kern = msver->get_kernel_object_owner();

        ss = kern->create<service::session>(reinterpret_cast<server_ptr>(msver->fserver_), 4);
        if (!ss) {
            LOG_ERROR(SERVICE_MSV, "Can't establish a new session for opening file");
            return nullptr;
        }

        fs_server_client *fclient = msver->fserver_->get_correspond_client(ss);
        if (!fclient) {
            kern->destroy(ss);

            LOG_ERROR(SERVICE_MSV, "Can't establish a client-side file server session!");
            return nullptr;
        }

        return fclient;
    }

    void msv_client_session::open_file_store_for_read(service::ipc_context *ctx) {
        msv_server *msver = server<msv_server>();
        kernel_system *kern = msver->get_kernel_object_owner();

        std::optional<std::uint32_t> id = ctx->get_argument_value<std::uint32_t>(0);
        if (!id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::msv::entry *ent = msver->indexer()->get_entry(id.value());
        if (!ent) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        service::session *ss = nullptr;
        fs_server_client *fclient = make_new_fs_client(ss);

        if (!fclient) {
            ctx->complete(epoc::error_not_ready);
            return;
        }

        std::optional<std::u16string> path = msver->indexer()->get_entry_data_file(ent);
        if (!path.has_value()) {
            LOG_ERROR(SERVICE_MSV, "Can't construct file store path for entry 0x{:X}", id.value());
            ctx->complete(epoc::error_general);

            msver->fserver_->disconnect_impl(ss);
            kern->destroy(ss);

            return;
        }

        const int res = fclient->new_node(msver->get_io_system(), ctx->msg->own_thr, path.value(), epoc::fs::file_read | epoc::fs::file_share_any,
            false, false, true);
        
        if (res < 0) {
            ctx->complete(res);

            msver->fserver_->disconnect_impl(ss);
            kern->destroy(ss);

            return;
        }

        kernel::handle h = kern->open_handle_with_thread(ctx->msg->own_thr, ss, kernel::owner_type::process);

        ctx->write_data_to_descriptor_argument<int>(1, res);
        ctx->complete(h);
    }

    static constexpr const char16_t *TEMP_EXT_STR = u".new";

    void msv_client_session::open_temp_file_store(service::ipc_context *ctx) {
        msv_server *msver = server<msv_server>();
        kernel_system *kern = msver->get_kernel_object_owner();

        std::optional<std::uint32_t> id = ctx->get_argument_value<std::uint32_t>(0);
        if (!id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::msv::entry *ent = msver->indexer()->get_entry(id.value());
        if (!ent) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        service::session *ss = nullptr;
        fs_server_client *fclient = make_new_fs_client(ss);

        if (!fclient) {
            ctx->complete(epoc::error_not_ready);
            return;
        }

        std::optional<std::u16string> path = msver->indexer()->get_entry_data_file(ent);
        if (!path.has_value()) {
            LOG_ERROR(SERVICE_MSV, "Can't construct file store path for entry 0x{:X}", id.value());
            ctx->complete(epoc::error_general);

            msver->fserver_->disconnect_impl(ss);
            kern->destroy(ss);

            return;
        }

        path.value() += TEMP_EXT_STR;

        const int res = fclient->new_node(msver->get_io_system(), ctx->msg->own_thr, path.value(), epoc::fs::file_write | epoc::fs::file_share_any,
            true, false, true);
        
        if (res < 0) {
            ctx->complete(res);

            msver->fserver_->disconnect_impl(ss);
            kern->destroy(ss);

            return;
        }

        kernel::handle h = kern->open_handle_with_thread(ctx->msg->own_thr, ss, kernel::owner_type::process);

        ctx->write_data_to_descriptor_argument<int>(1, res);
        ctx->complete(h);
    }

    void msv_client_session::replace_file_store(service::ipc_context *ctx) {
        msv_server *msver = server<msv_server>();

        std::optional<std::uint32_t> id = ctx->get_argument_value<std::uint32_t>(0);
        if (!id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::msv::entry *ent = msver->indexer()->get_entry(id.value());
        if (!ent) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        std::optional<std::u16string> path = msver->indexer()->get_entry_data_file(ent);
        if (!path.has_value()) {
            LOG_ERROR(SERVICE_MSV, "Can't construct file store path for entry 0x{:X}", id.value());
            ctx->complete(epoc::error_general);
            return;
        }

        io_system *io = msver->get_io_system();
        io->delete_entry(path.value());

        if (!io->rename(path.value() + TEMP_EXT_STR, path.value())) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    static void absorb_msv_capabilities(common::chunkyseri &seri, epoc::capability_set &set) {
        std::uint32_t version = 1;
        seri.absorb(version);

        std::string buffer(reinterpret_cast<char*>(set.bytes_), set.total_size);
        epoc::absorb_des_string(buffer, seri, false);
    }

    void msv_client_session::required_capabilities(service::ipc_context *ctx) {
        // SendAs server use this to verify if the sender has right permission to send without
        // user consent backgroundly. If this capabilities check fails, it will use Notifier to show
        // a dialogue asking user to confirm the send.
        std::uint8_t *buffer = ctx->get_descriptor_argument_ptr(1);
        std::size_t buffer_size = ctx->get_argument_max_data_size(1);

        if (!buffer || !buffer_size) {
            ctx->complete(epoc::error_argument);
            return;
        }
        
        epoc::capability_set set;
        set.clear();

        common::chunkyseri seri(buffer, buffer_size, common::SERI_MODE_WRITE);
        absorb_msv_capabilities(seri, set);

        ctx->set_descriptor_argument_length(1, static_cast<std::uint32_t>(seri.size()));
        ctx->complete(epoc::error_none);
    }

    void msv_client_session::file_store_exists(service::ipc_context *ctx) {
        msv_server *msver = server<msv_server>();

        std::optional<std::uint32_t> id = ctx->get_argument_value<std::uint32_t>(0);
        if (!id.has_value()) {
            ctx->complete(0);
            return;
        }

        epoc::msv::entry *ent = msver->indexer()->get_entry(id.value());
        if (!ent) {
            ctx->complete(0);
            return;
        }

        std::optional<std::u16string> path = msver->indexer()->get_entry_data_file(ent);
        if (!path.has_value()) {
            LOG_ERROR(SERVICE_MSV, "Can't construct file store path for entry 0x{:X}", id.value());
            ctx->complete(0);
            return;
        }

        io_system *io = msver->get_io_system();
        bool result = io->exist(path.value());

        ctx->complete(static_cast<int>(result));
    }

    void msv_client_session::operation_mtm(service::ipc_context *ctx) {
        std::optional<std::uint32_t> id1 = ctx->get_argument_value<std::uint32_t>(0);
        std::optional<std::uint32_t> id2 = ctx->get_argument_value<std::uint32_t>(1);

        if (!id1.has_value() || !id2.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        msv_server *srv = server<msv_server>();
        epoc::msv::entry *ent1 = srv->indexer()->get_entry(id1.value());

        if (!ent1) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        std::uint32_t mtm = 0;
        std::uint32_t service = 0;

        if (id1.value() == id2.value()) {
            std::uint32_t owning = 0;
            if (!srv->indexer()->owning_service(id1.value(), owning)) {
                LOG_ERROR(SERVICE_MSV, "Operation MTM owning service for id 0x{:X} can't be retrieved", id1.value());

                ctx->complete(epoc::error_not_found);
                return;
            }

            if (owning == epoc::msv::MSV_LOCAL_SERVICE_ID_VALUE) {
                mtm = epoc::msv::MSV_LOCAL_SERVICE_MTM_UID;
                service = epoc::msv::MSV_LOCAL_SERVICE_ID_VALUE;
            } else {
                mtm = ent1->mtm_uid_;
                service = ent1->service_id_;
            }
        } else {  
            epoc::msv::entry *ent2 = srv->indexer()->get_entry(id2.value());

            if (!ent2) {
                ctx->complete(epoc::error_not_found);
                return;
            }
            
            std::uint32_t owning1 = 0;
            std::uint32_t owning2 = 0;

            if (!srv->indexer()->owning_service(id1.value(), owning1)) {
                LOG_ERROR(SERVICE_MSV, "Operation MTM owning service for id 0x{:X} can't be retrieved", id1.value());

                ctx->complete(epoc::error_not_found);
                return;
            }

            if (!srv->indexer()->owning_service(id2.value(), owning2)) {
                LOG_ERROR(SERVICE_MSV, "Operation MTM owning service for id 0x{:X} can't be retrieved", id2.value());

                ctx->complete(epoc::error_not_found);
                return;
            }

            if ((owning1 == epoc::msv::MSV_LOCAL_SERVICE_ID_VALUE) && (owning2 == epoc::msv::MSV_LOCAL_SERVICE_ID_VALUE)) {
                mtm = epoc::msv::MSV_LOCAL_SERVICE_MTM_UID;
                service = epoc::msv::MSV_LOCAL_SERVICE_ID_VALUE;
            } else if (ent1->mtm_uid_ == ent2->mtm_uid_) {
                mtm = ent1->mtm_uid_;
                service = ent1->service_id_;
            } else if (ent1->mtm_uid_ == epoc::msv::MSV_LOCAL_SERVICE_MTM_UID) {
                // Entry 1 is local, entry 2 is MTM-dependent. Same comment as in original source code
                mtm = ent2->mtm_uid_;
                service = ent2->mtm_uid_;
            } else {
                mtm = ent1->mtm_uid_;
                service = ent1->service_id_;
            }
        }

        ctx->write_data_to_descriptor_argument<std::uint32_t>(2, mtm);
        ctx->write_data_to_descriptor_argument<std::uint32_t>(3, service);

        ctx->complete(epoc::error_none);
    }

    void msv_client_session::cancel_operation(service::ipc_context *ctx) {
        std::optional<std::uint32_t> operation_id = ctx->get_argument_value<std::uint32_t>(0);
        
        if (!operation_id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        for (std::size_t i = 0; i < operations_.size(); i++) {
            if (operations_[i]->operation_id() == operation_id.value()) {
                operations_[i]->cancel();

                if (operations_[i]->state() != epoc::msv::operation_state_queued) {
                    const epoc::msv::operation_buffer &buffer = operations_[i]->progress_buffer();

                    ctx->write_data_to_descriptor_argument(1, buffer.data(), static_cast<std::uint32_t>(buffer.size()));
                    ctx->complete(static_cast<int>(buffer.size()));
                } else {
                    ctx->complete(epoc::error_none);
                }

                operations_.erase(operations_.begin() + i);

                return;
            }
        }

        LOG_ERROR(SERVICE_MSV, "Progress buffer for operation 0x{:X} can't be found!", operation_id.value());
        ctx->complete(epoc::error_not_found);
    }
}
