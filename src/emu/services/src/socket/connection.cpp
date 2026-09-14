/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <services/socket/common.h>
#include <services/socket/connection.h>
#include <services/socket/server.h>
#include <services/socket/socket.h>
#include <services/centralrepo/centralrepo.h>

#include <common/log.h>
#include <utils/err.h>
#include <system/epoc.h>
#include <algorithm>
#include <cstring>
#include <iterator>

namespace eka2l1::epoc::socket {
    connection_state::~connection_state() {
        if (active) {
            advance(conn_progress_link_layer_closed);
            advance(conn_progress_connection_closed);
        }
    }

    void connection_state::advance(std::int32_t new_stage) {
        stage = new_stage;
        active = new_stage == conn_progress_connection_opened || new_stage == conn_progress_link_layer_open;
        const auto callbacks = observers;
        for (const auto &[owner, callback] : callbacks) {
            if (observers.count(owner)) {
                callback(new_stage);
            }
        }
    }

    std::shared_ptr<connection_state> connection_registry::create() {
        states_.erase(std::remove_if(states_.begin(), states_.end(),
            [](const auto &state) { return state.expired(); }), states_.end());
        auto state = std::make_shared<connection_state>();
        states_.push_back(state);
        return state;
    }

    std::vector<connection_info> connection_registry::enumerate() {
        std::vector<connection_info> result;
        for (const auto &weak : states_) {
            auto state = weak.lock();
            if (state && state->active && std::none_of(result.begin(), result.end(), [&](const auto &info) {
                    return info.iap_id == state->info.iap_id && info.network_id == state->info.network_id;
                })) {
                result.push_back(state->info);
            }
        }
        return result;
    }

    std::shared_ptr<connection_state> connection_registry::find(const connection_info &info) {
        for (const auto &weak : states_) {
            auto state = weak.lock();
            if (state && state->active && state->info.iap_id == info.iap_id && state->info.network_id == info.network_id) {
                return state;
            }
        }
        return nullptr;
    }

    bool connection_reference::enable_clone(const std::string &data) {
        if (data.size() != sizeof(security_policy)) {
            return false;
        }
        security_policy policy;
        std::memcpy(&policy, data.data(), sizeof(policy));
        if (policy.type > security_policy::v3) {
            return false;
        }
        const auto valid_cap = [](std::uint8_t cap) { return cap < cap_limit || cap == 0xFF; };
        if ((policy.type >= security_policy::c3 && !std::all_of(policy.caps, policy.caps + 3, valid_cap))
            || (policy.type == security_policy::c7 && !std::all_of(policy.extra_caps, policy.extra_caps + 4, valid_cap))) {
            return false;
        }
        clone_policy = policy;
        clone_enabled = true;
        return true;
    }

    std::shared_ptr<connection_reference> connection_registry::create_reference() {
        for (auto it = references_.begin(); it != references_.end();) {
            it = it->second.expired() ? references_.erase(it) : std::next(it);
        }
        auto reference = std::make_shared<connection_reference>();
        const auto number = std::to_string(++next_reference_);
        reference->name = u"Connection_" + std::u16string(number.begin(), number.end());
        references_[reference->name] = reference;
        return reference;
    }

    std::int32_t connection_registry::clone(const std::u16string &name, const security_info &caller,
        std::shared_ptr<connection_state> &state) {
        state.reset();
        const auto found = references_.find(name);
        auto reference = found == references_.end() ? nullptr : found->second.lock();
        if (!reference) {
            return epoc::error_not_found;
        }
        security_info missing;
        if (!reference->clone_enabled || !reference->clone_policy.check(caller, missing)) {
            return epoc::error_permission_denied;
        }
        auto source = reference->state.lock();
        if (!source || !source->active) {
            return epoc::error_not_ready;
        }
        state = std::move(source);
        return epoc::error_none;
    }

    connection::connection(protocol *pr, saddress dest)
        : pr_(pr)
        , sock_(nullptr)
        , dest_(dest) {
    }

    std::size_t connection::register_progress_advance_callback(progress_advance_callback cb) {
        return progress_callbacks_.add(cb);
    }

    bool connection::remove_progress_advance_callback(const std::size_t handle) {
        return progress_callbacks_.remove(handle);
    }

    socket_connection_proxy::socket_connection_proxy(socket_client_session *parent, connection *conn)
        : socket_subsession(parent)
        , conn_(conn)
        , progress_reported_(false) {
        reference_ = parent_->server<socket_server>()->connections().create_reference();
    }

    socket_connection_proxy::~socket_connection_proxy() {
        if (auto state = observed_state_.lock()) {
            state->observers.erase(this);
        }
        cancel_progress();
    }

    void socket_connection_proxy::bind_state(const std::shared_ptr<connection_state> &state, bool monitor) {
        if (auto previous = observed_state_.lock()) {
            previous->observers.erase(this);
        }
        observed_state_ = state;
        reference_->state = state;
        state_ = monitor ? nullptr : state;
        state->observers[this] = [this](std::int32_t stage) { set_progress(stage); };
        set_progress(state->stage);
    }

    std::int32_t socket_connection_proxy::clone_from(const std::u16string &name, const security_info &caller) {
        std::shared_ptr<connection_state> state;
        const auto result = parent_->server<socket_server>()->connections().clone(name, caller, state);
        if (result == epoc::error_none) {
            bind_state(state);
        }
        return result;
    }

    void socket_connection_proxy::name(service::ipc_context *ctx) {
        ctx->complete(ctx->write_arg(0, reference_->name) ? epoc::error_none : epoc::error_argument);
    }

    void socket_connection_proxy::control(service::ipc_context *ctx) {
        std::optional<std::uint32_t> level;
        std::optional<std::uint32_t> option;
        if (ctx->sys->get_symbian_version_use() < epocver::epoc95) {
            // Pre-reform Control packages the option in slot 0 and puts the level in slot 1.
            const auto data = ctx->get_argument_value<std::string>(0);
            if (!data || data->size() != sizeof(connection_control_description)) {
                ctx->complete(epoc::error_argument);
                return;
            }
            connection_control_description description;
            std::memcpy(&description, data->data(), sizeof(description));
            option = description.option;
            level = ctx->get_argument_value<std::uint32_t>(1);
        } else {
            level = ctx->get_argument_value<std::uint32_t>(0);
            option = ctx->get_argument_value<std::uint32_t>(1);
        }
        if (level != 1 || (option != 0x20000005 && option != 0x20000006)) {
            ctx->complete(epoc::error_not_supported);
            return;
        }
        if (option == 0x20000006) {
            reference_->clone_enabled = false;
        } else {
            const auto policy = ctx->get_argument_value<std::string>(2);
            if (!policy || !reference_->enable_clone(*policy)) {
                ctx->complete(epoc::error_argument);
                return;
            }
        }
        ctx->complete(epoc::error_none);
    }

    void socket_connection_proxy::attach(service::ipc_context *ctx) {
        const auto type = ctx->get_argument_value<std::uint32_t>(0);
        const auto data = ctx->get_argument_value<std::string>(1);
        if (!type || *type > 1 || !data || data->size() != sizeof(connection_info)) {
            ctx->complete(epoc::error_argument);
            return;
        }
        connection_info info;
        std::memcpy(&info, data->data(), sizeof(info));
        if ((info.version & 0xFF) != 1) {
            ctx->complete(epoc::error_not_supported);
            return;
        }
        auto state = parent_->server<socket_server>()->connections().find(info);
        if (!state) {
            ctx->complete(epoc::error_not_found);
            return;
        }
        // Monitor attachments observe the interface without extending its lifetime.
        bind_state(state, *type == 1);
        ctx->complete(epoc::error_none);
    }

    void socket_connection_proxy::cancel_progress() {
        if (progress_request_) {
            if (progress_request_->sys->get_kernel_system()->is_thread_alive(progress_request_->msg->own_thr)) {
                progress_request_->complete(epoc::error_cancel);
            }
            progress_request_.reset();
        }
    }

    void socket_connection_proxy::set_progress(std::int32_t stage) {
        progress_ = {stage, 0};
        progress_reported_ = false;
        if (progress_request_) {
            auto request = std::move(progress_request_);
            if (request->sys->get_kernel_system()->is_thread_alive(request->msg->own_thr)) {
                progress_notify(request.get());
            }
        }
    }

    void socket_connection_proxy::progress_notify(service::ipc_context *ctx) {
        if (progress_request_) {
            ctx->complete(epoc::error_in_use);
            return;
        }
        if (ctx->get_argument_max_data_size(0) < sizeof(progress_)) {
            ctx->complete(epoc::error_argument);
            return;
        }
        const auto selected = ctx->get_argument_value<std::int32_t>(1).value_or(0);
        if (!progress_reported_ && progress_.stage_ && (!selected || selected == progress_.stage_)) {
            ctx->complete(ctx->write_data_to_descriptor_argument(0, progress_) ? epoc::error_none : epoc::error_argument);
            progress_reported_ = true;
        } else {
            progress_request_ = ctx->move_to_new();
        }
    }

    void socket_connection_proxy::start(service::ipc_context *ctx, bool with_preferences) {
        std::uint32_t iap = 0;
        if (with_preferences) {
            const auto preferences = ctx->get_argument_value<std::string>(0);
            if (!preferences || preferences->size() < 24) {
                ctx->complete(epoc::error_argument);
                return;
            }
            std::uint16_t extension = 0;
            std::memcpy(&extension, preferences->data(), sizeof(extension));
            if (extension != 1) {
                ctx->complete(epoc::error_not_supported);
                return;
            }
            // TConnPref's four-byte header precedes SCommDbConnPref (commdbconnpref.h).
            std::memcpy(&iap, preferences->data() + 4, sizeof(iap));
        }

        auto *cenrep = reinterpret_cast<central_repo_server *>(ctx->sys->get_kernel_system()
            ->get_by_name<service::server>(CENTRAL_REPO_SERVER_NAME));
        auto *repo = cenrep ? cenrep->load_repo_with_lookup(ctx->sys->get_io_system(),
            ctx->sys->get_device_manager(), 0xCCCCCC00) : nullptr;
        if (!repo) {
            ctx->complete(epoc::error_not_found);
            return;
        }
        repo->access_count--;
        if (!iap) {
            for (std::uint32_t id = 1; id < 255; id++) {
                if (repo->find_entry(0x02820000 | (id << 8))) {
                    iap = id;
                    break;
                }
            }
        }
        auto *network = iap && iap < 255 ? repo->find_entry(0x02870000 | (iap << 8)) : nullptr;
        if (!network || network->data.etype != central_repo_entry_type::integer) {
            ctx->complete(epoc::error_not_found);
            return;
        }
        const connection_info info{1, iap, static_cast<std::uint32_t>(network->data.intd)};
        auto &registry = parent_->server<socket_server>()->connections();
        auto state = registry.find(info);
        if (!state) {
            state = registry.create();
            state->info = info;
        }
        bind_state(state);
        if (!state->active) {
            state->advance(conn_progress_connection_opened);
            state->advance(conn_progress_link_layer_open);
        }
        ctx->complete(epoc::error_none);
    }

    void socket_connection_proxy::enumerate(service::ipc_context *ctx) {
        snapshot_ = parent_->server<socket_server>()->connections().enumerate();
        ctx->complete(ctx->write_data_to_descriptor_argument(0, static_cast<std::uint32_t>(snapshot_.size()))
            ? epoc::error_none : epoc::error_argument);
    }

    void socket_connection_proxy::get_info(service::ipc_context *ctx) {
        const auto index = ctx->get_argument_value<std::uint32_t>(0);
        if (!index || !*index || *index > snapshot_.size()) {
            ctx->complete(epoc::error_argument);
            return;
        }
        ctx->complete(ctx->write_data_to_descriptor_argument(1, snapshot_[*index - 1])
            ? epoc::error_none : epoc::error_argument);
    }

    void socket_connection_proxy::get_int_setting(service::ipc_context *ctx) {
        const auto name = ctx->get_argument_value<std::u16string>(0);
        const auto state = observed_state_.lock();
        if (!state || !state->active) {
            ctx->complete(epoc::error_not_ready);
        } else if (name == u"IAP\\Id" || name == u"IAP\\IAPNetwork") {
            const auto value = name == u"IAP\\Id" ? state->info.iap_id : state->info.network_id;
            ctx->complete(ctx->write_data_to_descriptor_argument(1, value) ? epoc::error_none : epoc::error_argument);
        } else {
            ctx->complete(epoc::error_not_found);
        }
    }

    void socket_connection_proxy::get_des_setting(service::ipc_context *ctx) {
        const auto name = ctx->get_argument_value<std::u16string>(0);
        const auto state = observed_state_.lock();
        if (!state || !state->active) {
            ctx->complete(epoc::error_not_ready);
            return;
        }
        std::uint32_t field = 0;
        if (name == u"IAP\\Name") {
            field = 0x02820000;
        } else if (name == u"IAP\\IAPServiceType") {
            field = 0x02830000;
        } else if (name == u"IAP\\IAPBearerType") {
            field = 0x02850000;
        }
        auto *cenrep = reinterpret_cast<central_repo_server *>(ctx->sys->get_kernel_system()
            ->get_by_name<service::server>(CENTRAL_REPO_SERVER_NAME));
        auto *repo = field && cenrep ? cenrep->load_repo_with_lookup(ctx->sys->get_io_system(),
            ctx->sys->get_device_manager(), 0xCCCCCC00) : nullptr;
        if (repo) {
            repo->access_count--;
        }
        const auto *entry = repo ? repo->find_entry(field | (state->info.iap_id << 8)) : nullptr;
        if (!entry || entry->data.etype != central_repo_entry_type::string) {
            ctx->complete(epoc::error_not_found);
            return;
        }
        const auto &value = entry->data.strd;
        if (value.size() > ctx->get_argument_max_data_size(1)) {
            ctx->complete(epoc::error_overflow);
            return;
        }
        ctx->complete(ctx->write_data_to_descriptor_argument(1,
            reinterpret_cast<const std::uint8_t *>(value.data()), static_cast<std::uint32_t>(value.size()))
            ? epoc::error_none : epoc::error_argument);
    }

    void socket_connection_proxy::dispatch(service::ipc_context *ctx) {
        if (parent_->is_oldarch()) {
            switch (ctx->msg->function) {
            
            default:
                LOG_ERROR(SERVICE_ESOCK, "Unimplemented socket connection opcode: {}", ctx->msg->function);
                ctx->complete(epoc::error_none);

                break;
            }
        } else {
            if (ctx->sys->get_symbian_version_use() >= epocver::epoc95) {
                switch (ctx->msg->function) {
                case socket_reform_cn_name:
                    name(ctx);
                    break;

                case socket_reform_cn_control:
                    control(ctx);
                    break;

                default:
                    LOG_ERROR(SERVICE_ESOCK, "Unimplemented socket connection opcode: {}", ctx->msg->function);
                    ctx->complete(epoc::error_none);

                    break;
                }
            } else {
                switch (ctx->msg->function) {
                case socket_cn_name:
                    name(ctx);
                    break;

                case socket_cn_control:
                    control(ctx);
                    break;

                case socket_cn_close:
                    parent_->subsessions_.remove(id_);
                    ctx->complete(epoc::error_none);
                    break;

                case socket_cn_start_default:
                case socket_cn_start:
                    start(ctx, ctx->msg->function == socket_cn_start);
                    break;

                case socket_cn_enumerate_connections:
                    enumerate(ctx);
                    break;

                case socket_cn_get_connection_info:
                    get_info(ctx);
                    break;

                case socket_cn_get_int_setting:
                    get_int_setting(ctx);
                    break;

                case socket_cn_get_des_setting:
                    get_des_setting(ctx);
                    break;

                case socket_cn_attach:
                    attach(ctx);
                    break;

                case socket_cn_stop: {
                    const auto state = observed_state_.lock();
                    if (!state || !state->active) {
                        ctx->complete(epoc::error_not_ready);
                        break;
                    }
                    state->advance(conn_progress_link_layer_closed);
                    state->advance(conn_progress_connection_closed);
                    ctx->complete(epoc::error_none);
                    break;
                }

                case socket_cn_progress:
                    ctx->complete(ctx->write_data_to_descriptor_argument(0, progress_) ? epoc::error_none : epoc::error_argument);
                    break;

                case socket_cn_last_progress_error:
                    ctx->complete(ctx->write_data_to_descriptor_argument(0, conn_progress{}) ? epoc::error_none : epoc::error_argument);
                    break;

                case socket_cn_cancel_progress_notification:
                    cancel_progress();
                    ctx->complete(epoc::error_none);
                    break;

                case socket_cm_api_ext_interface_send_receive:
                    // Async, but we should complete it in sometimes
                    // Complete with not right result will create stuck or crash sometimes
                    break;

                case socket_cn_progress_notification:
                    progress_notify(ctx);
                    break;

                default:
                    LOG_ERROR(SERVICE_ESOCK, "Unimplemented socket connection opcode: {}", ctx->msg->function);
                    ctx->complete(epoc::error_none);

                    break;
                }
            }
        }
    }
}
