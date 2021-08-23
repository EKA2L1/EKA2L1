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
#include <system/epoc.h>
#include <vfs/vfs.h>

#include <utils/consts.h>
#include <utils/err.h>
#include <vfs/vfs.h>

#include <utils/bafl.h>

#include <common/cvt.h>
#include <common/time.h>

#include <system/devices.h>

#include <common/path.h>

namespace eka2l1 {
    static const std::u16string DEFAULT_MSG_DATA_DIR = u"C:\\private\\1000484b\\Mail2\\";

    std::string get_msv_server_name_by_epocver(const epocver ver) {
        if (ver < epocver::epoc81a) {
            return "MsvServer";
        }

        return "!MsvServer";
    }

    msv_server::msv_server(eka2l1::system *sys)
        : service::typical_server(sys, get_msv_server_name_by_epocver((sys->get_symbian_version_use())))
        , reg_(sys->get_io_system())
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

    void msv_server::init() {
        // Instantiate the message folder
        device_manager *mngr = sys->get_device_manager();
        message_folder_ = eka2l1::add_path(DEFAULT_MSG_DATA_DIR,
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
        for (auto &session: sessions) {
            msv_client_session *cli = reinterpret_cast<msv_client_session*>(session.second.get());
            cli->queue(evt);
        }
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

        if (buf->get_max_length(pr) < 3 * sizeof(std::uint32_t)) {
            LOG_ERROR(SERVICE_MSV, "Insufficent change buffer size");
            return;
        }

        buffer_raw[0] = static_cast<std::uint32_t>(data.nof_);
        buffer_raw[1] = data.arg1_;
        buffer_raw[2] = data.arg2_;

        buf->set_length(pr, 3 * sizeof(std::uint32_t));
    }

    bool msv_client_session::listen(epoc::notify_info &info, epoc::des8 *change, epoc::des8 *sel) {
        if (!msv_info_.empty()) {
            return false;
        }

        if (!events_.empty()) {
            msv_event_data evt = std::move(events_.front());
            events_.pop();

            kernel::process *own_pr = info.requester->owning_process();

            // Copy change
            pack_change_buffer(change, own_pr, evt);
            sel->assign(own_pr, evt.selection_);

            info.complete(epoc::error_none);
            return true;
        }

        msv_info_ = info;
        change_ = change;
        selection_ = sel;

        return true;
    }

    void msv_client_session::queue(msv_event_data &evt) {
        nof_sequence_++;
        evt.selection_ = std::string(reinterpret_cast<const char *>(&nof_sequence_), sizeof(std::uint32_t));

        if (!msv_info_.empty()) {
            kernel::process *own_pr = msv_info_.requester->owning_process();

            // Copy change
            pack_change_buffer(change_, own_pr, evt);
            selection_->assign(own_pr, evt.selection_);

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

        case msv_change_entry:
            change_entry(ctx);
            break;

        case msv_create_entry:
            create_entry(ctx);
            break;

        case msv_operation_progress:
            operation_info(ctx, false);
            break;

        case msv_operation_completion:
            operation_info(ctx, true);
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

    static void pad_out_data(common::chunkyseri &seri) {
        std::uint8_t padding[3];

        // Pad out
        if (seri.size() % 4 != 0) {
            seri.absorb_impl(padding, 4 - seri.size() % 4);
        }
    }

    static void absorb_entry_to_buffer(common::chunkyseri &seri, epoc::msv::entry &ent) {
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

        seri.absorb_impl(reinterpret_cast<std::uint8_t *>(&data_str), sizeof(epoc::msv::entry_data));

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

    static void absorb_entry_and_service_id_to_buffer(common::chunkyseri &seri, epoc::msv::entry &ent) {
        absorb_entry_to_buffer(seri, ent);
        seri.absorb(ent.service_id_);
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

        common::chunkyseri seri(buffer, buffer_max_size, common::SERI_MODE_MEASURE);
        absorb_entry_and_service_id_to_buffer(seri, *ent);

        if (seri.size() > buffer_max_size) {
            ctx->complete(epoc::error_overflow);
            return;
        }

        seri = common::chunkyseri(buffer, buffer_max_size, common::SERI_MODE_WRITE);
        absorb_entry_and_service_id_to_buffer(seri, *ent);

        ctx->set_descriptor_argument_length(1, static_cast<std::uint32_t>(seri.size()));
        
        // Slot 2 is usually service ID on old MSV
        if (ctx->get_descriptor_argument_ptr(2)) {
            ctx->write_data_to_descriptor_argument<std::uint32_t>(2, ent->service_id_);
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

        details->child_count_total_ = static_cast<std::uint32_t>(child_entries_.size());

        std::size_t written = 0;
        common::chunkyseri seri(buffer, buffer_max_size, common::SERI_MODE_WRITE);

        bool is_overflow = false;

        while ((written < buffer_max_size) && (!child_entries_.empty())) {
            common::chunkyseri measurer(nullptr, 0x1000, common::SERI_MODE_MEASURE);
            absorb_entry_to_buffer(measurer, *child_entries_.back());

            if (measurer.size() + written > buffer_max_size) {
                is_overflow = true;
                break;
            }

            // Start writing to this buffer
            absorb_entry_to_buffer(seri, *child_entries_.back());

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
            operation_buffer &mee = operation_buffers_[operation_id.value()];
            mee.resize(operation_buffer_size);

            std::memcpy(mee.data(), operation_buffer_ptr, operation_buffer_size);
        }

        ctx->complete(epoc::error_none);
    }

    bool msv_client_session::claim_operation_buffer(const std::uint32_t operation_id, operation_buffer &buffer) {
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

        // TODO: Consider the second argument in newer version for security.
        if (!operation_id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        operation_buffer buffer;
        if (!claim_operation_buffer(operation_id.value(), buffer)) {
            LOG_ERROR(SERVICE_MSV, "No buffer correspond with operation ID 0x{:X} to create entry", operation_id.value());
            ctx->complete(epoc::error_argument);

            return;
        }

        epoc::msv::entry target_entry;

        common::chunkyseri seri(buffer.data(), buffer.size(), common::SERI_MODE_READ);
        absorb_entry_to_buffer(seri, target_entry);

        msv_server *serv = server<msv_server>();
        epoc::msv::entry *final_entry = serv->indexer_->add_entry(target_entry);
        
        epoc::msv::local_operation_progress progress;
        progress.operation_ = epoc::msv::local_operation_new;
        progress.number_of_entries_ = 1;

        if (!final_entry) {
            LOG_ERROR(SERVICE_MSV, "Error adding entry!");

            progress.number_failed_++;
            progress.number_remaining_ = 1;
            progress.error_ = epoc::error_general;
        } else {
            progress.mid_ = final_entry->id_;
            progress.number_completed_++;

            // Queue entry added event
            msv_event_data created;
            created.nof_ = epoc::msv::change_notification_type_entries_created;
            created.arg1_ = final_entry->id_;
            created.arg2_ = final_entry->parent_id_;

            serv->queue(created);
        }

        // Report progress
        operation_buffer &progress_buffer = progress_buffers_[operation_id.value()];
        progress_buffer.resize(sizeof(progress));
        std::memcpy(progress_buffer.data(), &progress, sizeof(progress));

        ctx->complete(epoc::error_none);
    }

    void msv_client_session::change_entry(service::ipc_context *ctx) {
        std::optional<std::uint32_t> operation_id = ctx->get_argument_value<std::uint32_t>(0);

        // TODO: Consider the second argument in newer version for security.
        if (!operation_id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        operation_buffer buffer;
        if (!claim_operation_buffer(operation_id.value(), buffer)) {
            LOG_ERROR(SERVICE_MSV, "No buffer correspond with operation ID 0x{:X} to create entry", operation_id.value());
            ctx->complete(epoc::error_argument);

            return;
        }

        epoc::msv::entry target_entry;

        common::chunkyseri seri(buffer.data(), buffer.size(), common::SERI_MODE_READ);
        absorb_entry_to_buffer(seri, target_entry);

        msv_server *serv = server<msv_server>();
        const bool result = serv->indexer_->change_entry(target_entry);
        
        epoc::msv::local_operation_progress progress;
        progress.operation_ = epoc::msv::local_operation_changed;
        progress.number_of_entries_ = 1;

        if (!result) {
            LOG_ERROR(SERVICE_MSV, "Error changing entry!");

            progress.number_failed_++;
            progress.number_remaining_ = 1;
            progress.error_ = epoc::error_general;
        } else {
            progress.mid_ = target_entry.id_;
            progress.number_completed_++;

            // Queue entry changed event
            msv_event_data created;
            created.nof_ = epoc::msv::change_notification_type_entries_changed;
            created.arg1_ = target_entry.id_;
            created.arg2_ = target_entry.parent_id_;

            serv->queue(created);
        }

        // Report progress
        operation_buffer &progress_buffer = progress_buffers_[operation_id.value()];
        progress_buffer.resize(sizeof(progress));
        std::memcpy(progress_buffer.data(), &progress, sizeof(progress));

        ctx->complete(epoc::error_none);
    }

    void msv_client_session::operation_info(service::ipc_context *ctx, const bool is_complete) {
        std::optional<std::uint32_t> operation_id = ctx->get_argument_value<std::uint32_t>(0);
        
        if (!operation_id.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        auto ite = progress_buffers_.find(operation_id.value());
        if (ite == progress_buffers_.end()) {
            LOG_ERROR(SERVICE_MSV, "Progress buffer for operation 0x{:X} is not ready!", operation_id.value());

            ctx->complete(epoc::error_not_ready);
            return;
        }

        ctx->write_data_to_descriptor_argument(1, ite->second.data(), static_cast<std::uint32_t>(ite->second.size()));
        ctx->complete(static_cast<int>(ite->second.size()));

        if (is_complete) {
            progress_buffers_.erase(ite);
        }
    }
}
