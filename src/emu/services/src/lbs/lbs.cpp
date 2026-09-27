/*
 * Copyright (c) 2026 EKA2L1 Team
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

#include <services/lbs/lbs.h>
#include <services/context.h>

#include <common/log.h>
#include <kernel/kernel.h>
#include <kernel/timing.h>
#include <system/epoc.h>
#include <utils/err.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace eka2l1 {
    namespace {
        // S60 3.x lbs.dll numbers requests from 0x1000 and sends a Connect request; the
        // Symbian^3 one numbers them from 0x40000000 and has no Connect.
        constexpr std::uint32_t S60_SERVER_BASE = 0x1000;
        constexpr std::uint32_t S60_POSITIONER_BASE = 0x2000;
        constexpr std::uint32_t SYMBIAN_SERVER_BASE = 0x40000000;
        constexpr std::uint32_t SYMBIAN_POSITIONER_BASE = 0x40001000;
        constexpr std::uint32_t OPCODE_RANGE = 0x1000;

        // Server requests, numbered as S60 does.
        enum lbs_server_op {
            lbs_server_cancel_async_request = 0,
            lbs_server_connect = 1,
            lbs_server_get_default_module_id = 2,
            lbs_server_get_num_modules = 3,
            lbs_server_get_module_info_by_index = 4,
            lbs_server_get_module_info_by_id = 5,
            lbs_server_get_module_status = 6,
            lbs_server_notify_module_status_event = 7,
            lbs_server_empty_last_known_position_store = 8
        };

        enum lbs_positioner_op {
            lbs_positioner_cancel_async_request = 0,
            lbs_positioner_open = 1,
            lbs_positioner_open_module_id = 2,
            lbs_positioner_open_criteria = 3,
            lbs_positioner_close = 4,
            lbs_positioner_set_single_requestor = 5,
            lbs_positioner_set_multiple_requestors = 6,
            lbs_positioner_set_update_options = 7,
            lbs_positioner_get_update_options = 8,
            lbs_positioner_get_last_known_position = 9,
            lbs_positioner_notify_position_update = 10,
            lbs_positioner_get_last_known_position_area = 11
        };

        // CancelRequest() ids that predate the opcode numbering.
        constexpr std::uint32_t LEGACY_NOTIFY_MODULE_STATUS_EVENT_ID = 1006;
        constexpr std::uint32_t LEGACY_NOTIFY_POSITION_UPDATE_ID = 1110;

        constexpr std::int32_t POSITION_PARTIAL_UPDATE = 2;

        constexpr std::uint32_t MODULE_UID = 0x2000EA21;
        constexpr char16_t MODULE_NAME[] = u"EKA2L1 GPS";

        // A GPS receiver reports once per second whether it has a fix or not.
        constexpr std::int64_t RECEIVER_EPOCH_US = 1000000;

        // Offsets into the published LBS data classes, checked against the S60 3.2 and
        // Belle lbs.dll accessors.
        constexpr std::size_t CLASS_TYPE_OFFSET = 0;

        constexpr std::size_t MODULE_INFO_MODULE_ID_OFFSET = 8;
        constexpr std::size_t MODULE_INFO_IS_AVAILABLE_OFFSET = 12;
        constexpr std::size_t MODULE_INFO_NAME_OFFSET = 16;
        constexpr std::size_t MODULE_INFO_NAME_MAX_LENGTH = 64;
        constexpr std::size_t MODULE_INFO_TECHNOLOGY_OFFSET = 340;
        constexpr std::size_t MODULE_INFO_DEVICE_LOCATION_OFFSET = 344;
        constexpr std::size_t MODULE_INFO_CAPABILITIES_OFFSET = 348;
        constexpr std::size_t MODULE_INFO_CLASSES_OFFSET = 352;
        constexpr std::size_t MODULE_INFO_VERSION_OFFSET = 432;
        constexpr std::size_t MODULE_INFO_MIN_SIZE = MODULE_INFO_VERSION_OFFSET + 4;

        constexpr std::size_t MODULE_STATUS_DEVICE_STATUS_OFFSET = 8;
        constexpr std::size_t MODULE_STATUS_DATA_QUALITY_OFFSET = 12;
        constexpr std::size_t MODULE_STATUS_MIN_SIZE = 16;

        constexpr std::size_t UPDATE_OPTIONS_INTERVAL_OFFSET = 8;
        constexpr std::size_t UPDATE_OPTIONS_TIMEOUT_OFFSET = 16;
        constexpr std::size_t UPDATE_OPTIONS_MAX_AGE_OFFSET = 24;
        constexpr std::size_t UPDATE_OPTIONS_PARTIAL_OFFSET = 40;
        constexpr std::size_t UPDATE_OPTIONS_BASE_SIZE = 40;

        constexpr std::size_t POSITION_INFO_MODULE_ID_OFFSET = 8;
        constexpr std::size_t POSITION_INFO_UPDATE_TYPE_OFFSET = 12;
        constexpr std::size_t POSITION_INFO_BASE_SIZE = 32;
        constexpr std::size_t POSITION_LATITUDE_OFFSET = 32;
        constexpr std::size_t POSITION_LONGITUDE_OFFSET = 40;
        constexpr std::size_t POSITION_ALTITUDE_OFFSET = 48;
        constexpr std::size_t POSITION_TIME_OFFSET = 88;
        constexpr std::size_t POSITION_INFO_SIZE = 112;

        constexpr std::uint32_t POSITION_INFO_CLASS = 0x01;
        constexpr std::uint32_t POSITION_GENERIC_INFO_CLASS = 0x02;
        constexpr std::uint32_t POSITION_UPDATE_GENERAL = 0x01;

        constexpr std::size_t CLASS_FAMILY_COUNT = 7;
        constexpr std::uint32_t TECHNOLOGY_TERMINAL = 0x01;
        constexpr std::uint32_t DEVICE_INTERNAL = 0x01;
        constexpr std::uint32_t CAPABILITY_HORIZONTAL_VERTICAL = 0x03;
        constexpr std::uint32_t DEVICE_STATUS_READY = 6;
        constexpr std::uint32_t DATA_QUALITY_LOSS = 1;

        template <typename T>
        T get_value(const std::vector<std::uint8_t> &buf, const std::size_t offset) {
            T value{};
            std::memcpy(&value, buf.data() + offset, sizeof(T));
            return value;
        }

        template <typename T>
        void put_value(std::vector<std::uint8_t> &buf, const std::size_t offset, const T value) {
            std::memcpy(buf.data() + offset, &value, sizeof(T));
        }

        std::vector<std::uint8_t> read_descriptor(service::ipc_context *ctx, const int idx) {
            const std::uint8_t *data = ctx->get_descriptor_argument_ptr(idx);
            if (!data) {
                return {};
            }

            return std::vector<std::uint8_t>(data, data + ctx->get_argument_data_size(idx));
        }

        bool write_descriptor(service::ipc_context *ctx, const int idx, const std::vector<std::uint8_t> &buf) {
            return ctx->write_data_to_descriptor_argument(idx, buf.data(), static_cast<std::uint32_t>(buf.size()));
        }

        bool is_our_module(service::ipc_context *ctx) {
            std::optional<std::uint32_t> module_id = ctx->get_argument_data_from_descriptor<std::uint32_t>(0);
            return module_id && (module_id.value() == MODULE_UID);
        }
    }

    lbs_server::lbs_server(eka2l1::system *sys)
        : service::typical_server(sys, (sys->get_symbian_version_use() >= epocver::epoc95) ? "!PosServer" : "PosServer")
        , kern_(sys->get_kernel_system())
        , timing_(sys->get_ntimer()) {
        update_evt_ = timing_->register_event("LbsPositionUpdate", [this](std::uint64_t userdata, int) {
            kern_->lock();

            if (!kern_->is_wiping()) {
                complete_update(userdata);
            }

            kern_->unlock();
        });
    }

    lbs_server::~lbs_server() {
        timing_->remove_event(update_evt_);
    }

    void lbs_server::connect(service::ipc_context &context) {
        create_session<lbs_client_session>(&context);
        context.complete(epoc::error_none);
    }

    std::unique_ptr<lbs_positioner> lbs_server::new_positioner() {
        auto positioner = std::make_unique<lbs_positioner>();
        positioner->id_ = ++positioner_id_counter_;
        positioners_.emplace(positioner->id_, positioner.get());

        return positioner;
    }

    void lbs_server::close_positioner(lbs_positioner *positioner) {
        timing_->unschedule_event(update_evt_, positioner->id_);
        positioners_.erase(positioner->id_);
    }

    void lbs_server::schedule_update(lbs_positioner *positioner) {
        const lbs_update_options &options = positioner->options_;
        const std::int64_t now = static_cast<std::int64_t>(timing_->microseconds());

        std::int64_t start = now;
        if (positioner->has_updated_ && (options.interval_ > 0)) {
            start = std::max(start, static_cast<std::int64_t>(positioner->last_update_time_) + options.interval_);
        }

        std::int64_t due = 0;

        if (options.accept_partial_) {
            due = std::max(start, now + RECEIVER_EPOCH_US);
            positioner->update_times_out_ = (options.timeout_ > 0) && (now + options.timeout_ < due);

            if (positioner->update_times_out_) {
                due = now + options.timeout_;
            }
        } else if (options.timeout_ > 0) {
            due = start + options.timeout_;
            positioner->update_times_out_ = true;
        } else {
            // Without partial updates or a timeout the request waits for a fix that never comes.
            return;
        }

        timing_->schedule_event(due - now, update_evt_, positioner->id_);
    }

    void lbs_server::cancel_update(lbs_positioner *positioner, const int code) {
        timing_->unschedule_event(update_evt_, positioner->id_);

        if (positioner->update_msg_) {
            positioner->update_msg_->complete(code);
            positioner->update_msg_.reset();
        }
    }

    void lbs_server::complete_update(const std::uint64_t id) {
        auto positioner_ite = positioners_.find(id);
        if (positioner_ite == positioners_.end()) {
            return;
        }

        lbs_positioner *positioner = positioner_ite->second;
        std::unique_ptr<service::ipc_context> msg = std::move(positioner->update_msg_);

        positioner->last_update_time_ = timing_->microseconds();
        positioner->has_updated_ = true;

        if (!msg || !msg->msg || !kern_->is_thread_alive(msg->msg->own_thr)) {
            return;
        }

        if (positioner->update_times_out_) {
            msg->complete(epoc::error_timed_out);
            return;
        }

        std::vector<std::uint8_t> info = read_descriptor(msg.get(), 0);
        if (info.size() < POSITION_INFO_BASE_SIZE) {
            msg->complete(epoc::error_argument);
            return;
        }

        put_value<std::uint32_t>(info, POSITION_INFO_MODULE_ID_OFFSET, MODULE_UID);
        put_value<std::uint32_t>(info, POSITION_INFO_UPDATE_TYPE_OFFSET, POSITION_UPDATE_GENERAL);

        if ((get_value<std::uint32_t>(info, CLASS_TYPE_OFFSET) & POSITION_INFO_CLASS) && (info.size() >= POSITION_INFO_SIZE)) {
            // A partial update only guarantees the timestamp.
            put_value<double>(info, POSITION_LATITUDE_OFFSET, std::numeric_limits<double>::quiet_NaN());
            put_value<double>(info, POSITION_LONGITUDE_OFFSET, std::numeric_limits<double>::quiet_NaN());
            put_value<float>(info, POSITION_ALTITUDE_OFFSET, std::numeric_limits<float>::quiet_NaN());
            put_value<std::uint64_t>(info, POSITION_TIME_OFFSET, kern_->universal_time());
        }

        write_descriptor(msg.get(), 0, info);
        msg->complete(POSITION_PARTIAL_UPDATE);
    }

    lbs_client_session::lbs_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    lbs_client_session::~lbs_client_session() {
        for (auto &positioner : positioners_) {
            server<lbs_server>()->close_positioner(positioner.second.get());
        }
    }

    void lbs_client_session::fetch(service::ipc_context *ctx) {
        const std::uint32_t func = ctx->msg->function;

        if ((func >= SYMBIAN_POSITIONER_BASE) && (func < SYMBIAN_POSITIONER_BASE + OPCODE_RANGE)) {
            fetch_positioner(ctx, func - SYMBIAN_POSITIONER_BASE);
        } else if ((func >= SYMBIAN_SERVER_BASE) && (func < SYMBIAN_SERVER_BASE + OPCODE_RANGE)) {
            const std::uint32_t op = func - SYMBIAN_SERVER_BASE;
            fetch_server(ctx, (op == lbs_server_cancel_async_request) ? op : op + 1);
        } else if ((func >= S60_POSITIONER_BASE) && (func < S60_POSITIONER_BASE + OPCODE_RANGE)) {
            fetch_positioner(ctx, func - S60_POSITIONER_BASE);
        } else if ((func >= S60_SERVER_BASE) && (func < S60_SERVER_BASE + OPCODE_RANGE)) {
            fetch_server(ctx, func - S60_SERVER_BASE);
        } else {
            LOG_ERROR(SERVICE_LBS, "Unimplemented location server opcode 0x{:X}", func);
            ctx->complete(epoc::error_not_supported);
        }
    }

    void lbs_client_session::fetch_server(service::ipc_context *ctx, const std::uint32_t op) {
        switch (op) {
        case lbs_server_cancel_async_request:
            cancel_server_request(ctx);
            break;

        case lbs_server_connect:
        case lbs_server_empty_last_known_position_store:
            ctx->complete(epoc::error_none);
            break;

        case lbs_server_get_default_module_id:
            get_default_module_id(ctx);
            break;

        case lbs_server_get_num_modules:
            ctx->write_data_to_descriptor_argument<std::uint32_t>(0, 1);
            ctx->complete(epoc::error_none);
            break;

        case lbs_server_get_module_info_by_index:
        case lbs_server_get_module_info_by_id:
            get_module_info(ctx, op == lbs_server_get_module_info_by_index);
            break;

        case lbs_server_get_module_status:
            get_module_status(ctx);
            break;

        case lbs_server_notify_module_status_event:
            notify_module_status_event(ctx);
            break;

        default:
            LOG_ERROR(SERVICE_LBS, "Unimplemented location server request {}", op);
            ctx->complete(epoc::error_not_supported);
            break;
        }
    }

    void lbs_client_session::fetch_positioner(service::ipc_context *ctx, const std::uint32_t op) {
        switch (op) {
        case lbs_positioner_open:
        case lbs_positioner_open_criteria:
            open_positioner(ctx, false);
            return;

        case lbs_positioner_open_module_id:
            open_positioner(ctx, true);
            return;

        default:
            break;
        }

        lbs_positioner *positioner = get_positioner(ctx);
        if (!positioner) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        switch (op) {
        case lbs_positioner_close:
            close_positioner(ctx);
            break;

        case lbs_positioner_cancel_async_request:
            cancel_positioner_request(ctx);
            break;

        case lbs_positioner_set_single_requestor:
        case lbs_positioner_set_multiple_requestors:
            ctx->complete(epoc::error_none);
            break;

        case lbs_positioner_set_update_options:
            set_update_options(ctx);
            break;

        case lbs_positioner_get_update_options:
            get_update_options(ctx);
            break;

        case lbs_positioner_get_last_known_position:
        case lbs_positioner_get_last_known_position_area:
            // Nothing has ever been fixed.
            ctx->complete(epoc::error_unknown);
            break;

        case lbs_positioner_notify_position_update:
            notify_position_update(ctx);
            break;

        default:
            LOG_ERROR(SERVICE_LBS, "Unimplemented positioner request {}", op);
            ctx->complete(epoc::error_not_supported);
            break;
        }
    }

    lbs_positioner *lbs_client_session::get_positioner(service::ipc_context *ctx) {
        std::optional<std::uint32_t> handle = ctx->get_argument_value<std::uint32_t>(3);
        if (!handle) {
            return nullptr;
        }

        auto positioner_ite = positioners_.find(handle.value());
        return (positioner_ite == positioners_.end()) ? nullptr : positioner_ite->second.get();
    }

    void lbs_client_session::cancel_server_request(service::ipc_context *ctx) {
        std::optional<std::uint32_t> request = ctx->get_argument_value<std::uint32_t>(0);
        const bool is_status_event = request && ((request.value() == LEGACY_NOTIFY_MODULE_STATUS_EVENT_ID)
            || (request.value() == S60_SERVER_BASE + lbs_server_notify_module_status_event)
            || (request.value() == SYMBIAN_SERVER_BASE + lbs_server_notify_module_status_event - 1));

        if (!is_status_event) {
            ctx->complete(epoc::error_not_supported);
            return;
        }

        if (!status_event_msg_) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        status_event_msg_->complete(epoc::error_cancel);
        status_event_msg_.reset();

        ctx->complete(epoc::error_none);
    }

    void lbs_client_session::get_default_module_id(service::ipc_context *ctx) {
        ctx->write_data_to_descriptor_argument<std::uint32_t>(0, MODULE_UID);
        ctx->complete(epoc::error_none);
    }

    void lbs_client_session::get_module_info(service::ipc_context *ctx, const bool by_index) {
        const bool found = by_index ? (ctx->get_argument_value<std::uint32_t>(0) == 0u) : is_our_module(ctx);
        if (!found) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        std::vector<std::uint8_t> info = read_descriptor(ctx, 1);
        if (info.size() < MODULE_INFO_MIN_SIZE) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const std::u16string name = MODULE_NAME;

        put_value<std::uint32_t>(info, MODULE_INFO_MODULE_ID_OFFSET, MODULE_UID);
        put_value<std::uint32_t>(info, MODULE_INFO_IS_AVAILABLE_OFFSET, 1);

        // TBuf<KPositionMaxModuleName>: length word (type EBuf in the top bits), max length, text.
        put_value<std::uint32_t>(info, MODULE_INFO_NAME_OFFSET, static_cast<std::uint32_t>(name.length()) | (3u << 28));
        put_value<std::uint32_t>(info, MODULE_INFO_NAME_OFFSET + 4, MODULE_INFO_NAME_MAX_LENGTH);
        std::memcpy(info.data() + MODULE_INFO_NAME_OFFSET + 8, name.data(), name.length() * sizeof(char16_t));

        put_value<std::uint32_t>(info, MODULE_INFO_TECHNOLOGY_OFFSET, TECHNOLOGY_TERMINAL);
        put_value<std::uint32_t>(info, MODULE_INFO_DEVICE_LOCATION_OFFSET, DEVICE_INTERNAL);
        put_value<std::uint32_t>(info, MODULE_INFO_CAPABILITIES_OFFSET, CAPABILITY_HORIZONTAL_VERTICAL);

        // Every class family supports its standard class; position info also has the generic one.
        for (std::size_t i = 0; i < CLASS_FAMILY_COUNT; i++) {
            put_value<std::uint32_t>(info, MODULE_INFO_CLASSES_OFFSET + i * 4, (i == 0)
                ? (POSITION_INFO_CLASS | POSITION_GENERIC_INFO_CLASS) : 1u);
        }

        put_value<std::uint32_t>(info, MODULE_INFO_VERSION_OFFSET, 1);

        write_descriptor(ctx, 1, info);
        ctx->complete(epoc::error_none);
    }

    void lbs_client_session::get_module_status(service::ipc_context *ctx) {
        if (!is_our_module(ctx)) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        std::vector<std::uint8_t> status = read_descriptor(ctx, 1);
        if (status.size() < MODULE_STATUS_MIN_SIZE) {
            ctx->complete(epoc::error_argument);
            return;
        }

        put_value<std::uint32_t>(status, MODULE_STATUS_DEVICE_STATUS_OFFSET, DEVICE_STATUS_READY);
        put_value<std::uint32_t>(status, MODULE_STATUS_DATA_QUALITY_OFFSET, DATA_QUALITY_LOSS);

        write_descriptor(ctx, 1, status);
        ctx->complete(epoc::error_none);
    }

    void lbs_client_session::notify_module_status_event(service::ipc_context *ctx) {
        if (status_event_msg_) {
            ctx->complete(epoc::error_in_use);
            return;
        }

        // The module status never changes, so this only completes when cancelled.
        status_event_msg_ = ctx->move_to_new();
    }

    void lbs_client_session::open_positioner(service::ipc_context *ctx, const bool by_module_id) {
        if (by_module_id && !is_our_module(ctx)) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        const std::uint32_t handle = ++handle_counter_;
        positioners_.emplace(handle, server<lbs_server>()->new_positioner());

        ctx->write_data_to_descriptor_argument<std::uint32_t>(3, handle);
        ctx->complete(epoc::error_none);
    }

    void lbs_client_session::close_positioner(service::ipc_context *ctx) {
        auto positioner_ite = positioners_.find(ctx->get_argument_value<std::uint32_t>(3).value());
        lbs_server *serv = server<lbs_server>();

        serv->cancel_update(positioner_ite->second.get(), epoc::error_cancel);
        serv->close_positioner(positioner_ite->second.get());
        positioners_.erase(positioner_ite);

        ctx->complete(epoc::error_none);
    }

    void lbs_client_session::cancel_positioner_request(service::ipc_context *ctx) {
        lbs_positioner *positioner = get_positioner(ctx);
        std::optional<std::uint32_t> request = ctx->get_argument_value<std::uint32_t>(0);

        const bool is_update = request && ((request.value() == LEGACY_NOTIFY_POSITION_UPDATE_ID)
            || (request.value() == S60_POSITIONER_BASE + lbs_positioner_notify_position_update)
            || (request.value() == SYMBIAN_POSITIONER_BASE + lbs_positioner_notify_position_update));

        if (!is_update) {
            // Last known position requests complete immediately, so there is nothing to cancel.
            ctx->complete(epoc::error_not_found);
            return;
        }

        if (!positioner->update_msg_) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        server<lbs_server>()->cancel_update(positioner, epoc::error_cancel);
        ctx->complete(epoc::error_none);
    }

    void lbs_client_session::set_update_options(service::ipc_context *ctx) {
        std::vector<std::uint8_t> buf = read_descriptor(ctx, 0);
        if (buf.size() < UPDATE_OPTIONS_BASE_SIZE) {
            ctx->complete(epoc::error_argument);
            return;
        }

        lbs_update_options options;
        options.interval_ = get_value<std::int64_t>(buf, UPDATE_OPTIONS_INTERVAL_OFFSET);
        options.timeout_ = get_value<std::int64_t>(buf, UPDATE_OPTIONS_TIMEOUT_OFFSET);
        options.max_age_ = get_value<std::int64_t>(buf, UPDATE_OPTIONS_MAX_AGE_OFFSET);

        if (buf.size() >= UPDATE_OPTIONS_PARTIAL_OFFSET + 4) {
            options.accept_partial_ = (get_value<std::uint32_t>(buf, UPDATE_OPTIONS_PARTIAL_OFFSET) != 0);
        }

        if ((options.interval_ < 0) || (options.timeout_ < 0) || (options.max_age_ < 0)
            || ((options.timeout_ > 0) && (options.interval_ >= options.timeout_))
            || ((options.max_age_ != 0) && (options.interval_ != 0) && (options.max_age_ >= options.interval_))) {
            ctx->complete(epoc::error_argument);
            return;
        }

        get_positioner(ctx)->options_ = options;
        ctx->complete(epoc::error_none);
    }

    void lbs_client_session::get_update_options(service::ipc_context *ctx) {
        std::vector<std::uint8_t> buf = read_descriptor(ctx, 0);
        if (buf.size() < UPDATE_OPTIONS_BASE_SIZE) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const lbs_update_options &options = get_positioner(ctx)->options_;
        put_value<std::int64_t>(buf, UPDATE_OPTIONS_INTERVAL_OFFSET, options.interval_);
        put_value<std::int64_t>(buf, UPDATE_OPTIONS_TIMEOUT_OFFSET, options.timeout_);
        put_value<std::int64_t>(buf, UPDATE_OPTIONS_MAX_AGE_OFFSET, options.max_age_);

        if (buf.size() >= UPDATE_OPTIONS_PARTIAL_OFFSET + 4) {
            put_value<std::uint32_t>(buf, UPDATE_OPTIONS_PARTIAL_OFFSET, options.accept_partial_ ? 1 : 0);
        }

        write_descriptor(ctx, 0, buf);
        ctx->complete(epoc::error_none);
    }

    void lbs_client_session::notify_position_update(service::ipc_context *ctx) {
        lbs_positioner *positioner = get_positioner(ctx);

        if (positioner->update_msg_) {
            ctx->complete(epoc::error_in_use);
            return;
        }

        if (ctx->get_argument_data_size(0) < POSITION_INFO_BASE_SIZE) {
            ctx->complete(epoc::error_argument);
            return;
        }

        positioner->update_msg_ = ctx->move_to_new();
        server<lbs_server>()->schedule_update(positioner);
    }
}
