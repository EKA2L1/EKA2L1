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

#include <system/epoc.h>
#include <services/msv/msv.h>
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
        if (ver <= epocver::eka2) {
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

        auto mtm_module_dir = io->open_dir(DEFAULT_MSG_MTM_FILE_DIR, io_attrib_include_file);

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
            reg_.set_list_path(u"C:\\System\\mtm\\Mtm Registry v2");
        }

        reg_.load_mtm_list();
        install_rom_mtm_modules();

        indexer_ = std::make_unique<epoc::msv::entry_indexer>(io, message_folder_, sys->get_system_language());

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

    msv_client_session::msv_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version)
        , flags_(0)
        , nof_sequence_(0) {
    }

    static void pack_change_buffer(epoc::des8 *buf, kernel::process *pr, const msv_event_data &data) {
        std::uint32_t *buffer_raw = reinterpret_cast<std::uint32_t *>(buf->get_pointer_raw(pr));

        if (!buffer_raw) {
            LOG_ERROR("Can't get change buffer raw pointer!");
            return;
        }

        if (buf->get_max_length(pr) < 3 * sizeof(std::uint32_t)) {
            LOG_ERROR("Insufficent change buffer size");
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

        default: {
            LOG_ERROR("Unimplemented opcode for MsvServer 0x{:X}", ctx->msg->function);
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
            LOG_ERROR("Request listen on an already-listening MSV session");
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

    static void pack_entry_to_buffer(common::chunkyseri &seri, epoc::msv::entry &ent) {
        epoc::msv::entry_data data_str;
        data_str.data_ = ent.data_;
        data_str.date_ = common::convert_microsecs_epoch_to_1ad(ent.time_);
        data_str.id_ = ent.id_;
        data_str.parent_id_ = ent.parent_id_;
        data_str.service_id_ = ent.service_id_;
        data_str.mtm_uid_ = ent.mtm_uid_;
        data_str.type_uid_ = ent.type_uid_;

        seri.absorb_impl(reinterpret_cast<std::uint8_t *>(&data_str), sizeof(epoc::msv::entry_data));

        pad_out_data(seri);
        seri.absorb(ent.description_);

        pad_out_data(seri);
        seri.absorb(ent.details_);

        pad_out_data(seri);
    }

    static void pack_entry_and_service_id_to_buffer(common::chunkyseri &seri, epoc::msv::entry &ent) {
        pack_entry_to_buffer(seri, ent);
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
        pack_entry_to_buffer(seri, *ent);

        if (seri.size() > buffer_max_size) {
            ctx->complete(epoc::error_overflow);
            return;
        }

        seri = common::chunkyseri(buffer, buffer_max_size, common::SERI_MODE_WRITE);
        pack_entry_to_buffer(seri, *ent);

        ctx->set_descriptor_argument_length(1, static_cast<std::uint32_t>(seri.size()));
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

        if (child_entries_.empty()) {
            ctx->complete(epoc::error_not_found);
            return;
        }

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
            pack_entry_to_buffer(measurer, *child_entries_.back());

            if (measurer.size() + written > buffer_max_size) {
                is_overflow = true;
                break;
            }

            // Start writing to this buffer
            pack_entry_to_buffer(seri, *child_entries_.back());

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
            epoc::absorb_des_string(comp->name_, seri, true);
            seri.absorb(group->cap_send_);
            seri.absorb(group->cap_body_);
            seri.absorb(group->cap_avail_);

            std::uint32_t dll_uid = 0x10000079; ///< Dynamic DLL UID
            seri.absorb(dll_uid);
            seri.absorb(comp->comp_uid_);
            seri.absorb(comp->specific_uid_);

            std::uint32_t entry_point = comp->entry_point_;
            seri.absorb(entry_point);

            // Absorb versions
            std::uint32_t version = (comp->build_) | ((comp->major_ & 0xFF) << 16) | ((comp->minor_ & 0xFF) << 8);
            seri.absorb(version);

            if (comp->specific_uid_ == epoc::msv::MTM_DEFAULT_SPECIFIC_UID) {
                //std::string filename_utf8 = common::ucs2_to_utf8(comp->filename_);
                epoc::absorb_des_string(comp->filename_, seri, true);
            }
        }

        ctx->set_descriptor_argument_length(1, static_cast<std::uint32_t>(seri.size()) + 4);
        ctx->complete(epoc::error_none);
    }
}
