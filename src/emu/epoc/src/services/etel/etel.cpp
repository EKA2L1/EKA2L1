/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <epoc/services/etel/common.h>
#include <epoc/services/etel/etel.h>
#include <epoc/utils/err.h>

#include <common/cvt.h>

namespace eka2l1 {
    etel_server::etel_server(eka2l1::system *sys)
        : service::typical_server(sys, "!EtelServer") {
    }

    void etel_server::connect(service::ipc_context &ctx) {
        create_session<etel_session>(&ctx);
        ctx.set_request_status(epoc::error_none);
    }
    
    etel_session::etel_session(service::typical_server *serv, service::uid client_ss_uid, epoc::version client_ver)
        : service::typical_session(serv, client_ss_uid, client_ver) {
    }

    std::optional<std::uint32_t> etel_session::get_entry(const std::uint32_t org_index, const etel_entry_type type) {
        std::int32_t i = -1;

        while ((i != org_index) && ((i < 0) || (i < entries_.size())) && entries_[i].type_ != type)
            i++;

        if (i == entries_.size() || (i < 0)) {
            return std::nullopt;
        }

        return i;
    }

    void etel_session::load_phone_module(service::ipc_context *ctx) {
        std::optional<std::u16string> name = ctx->get_arg<std::u16string>(0);

        if (!name) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        LOG_TRACE("Stub loading phone module with name {}", common::ucs2_to_utf8(name.value()));

        // Add an assumed phone module
        etel_module_entry phone_entry;
        phone_entry.type_ = etel_entry_phone;

        phone_entry.tsy_name_ = common::ucs2_to_utf8(name.value());
        phone_entry.phone_.exts_ = 0;
        phone_entry.phone_.network_ = epoc::etel_network_type_mobile_digital;
        phone_entry.phone_.phone_name_.assign(nullptr, name.value());

        entries_.push_back(phone_entry);
        ctx->set_request_status(epoc::error_none);
    }

    void etel_session::enumerate_phones(service::ipc_context *ctx) {
        std::uint32_t total_phone = 0;

        for (std::size_t i = 0; i < entries_.size(); i++) {
            if (entries_[i].type_ == etel_entry_phone) {
                total_phone++;
            }
        }

        ctx->write_arg_pkg(0, total_phone);
        ctx->set_request_status(epoc::error_none);
    }

    void etel_session::get_phone_info_by_index(service::ipc_context *ctx) {
        epoc::etel_phone_info info;
        const std::int32_t index = *ctx->get_arg<std::int32_t>(1);
        std::optional<std::uint32_t> real_index = get_entry(index, etel_entry_phone);

        if (!real_index.has_value()) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        ctx->write_arg_pkg<epoc::etel_phone_info>(0, entries_[real_index.value()].phone_);
        ctx->set_request_status(epoc::error_none);
    }

    void etel_session::get_tsy_name(service::ipc_context *ctx) {
        const std::int32_t index = *ctx->get_arg<std::int32_t>(0);
        std::optional<std::uint32_t> real_index = get_entry(index, etel_entry_phone);

        if (!real_index.has_value()) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        ctx->write_arg(1, common::utf8_to_ucs2(entries_[real_index.value()].tsy_name_));
        ctx->set_request_status(epoc::error_none);
    }

    void etel_session::open_from_session(service::ipc_context *ctx) {
        LOG_TRACE("Open from etel session stubbed");

        const std::uint32_t dummy_handle = 0x43210;
        ctx->write_arg_pkg<std::uint32_t>(3, dummy_handle);
        ctx->set_request_status(epoc::error_none);
    }

    void etel_session::open_from_subsession(service::ipc_context *ctx) {
        LOG_TRACE("Open from etel subsession stubbed");

        const std::uint32_t dummy_handle = 0x43210;
        ctx->write_arg_pkg<std::uint32_t>(3, dummy_handle);
        ctx->set_request_status(epoc::error_none);
    }

    void etel_session::close_sub(service::ipc_context *ctx) {
        LOG_TRACE("Close etel subsession stubbed!");
        ctx->set_request_status(epoc::error_none);
    }

    void etel_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case epoc::etel_open_from_session:
            open_from_session(ctx);
            break;

        case epoc::etel_open_from_subsession:
            open_from_subsession(ctx);
            break;

        case epoc::etel_close:
            close_sub(ctx);
            break;

        case epoc::etel_enumerate_phones:
            enumerate_phones(ctx);
            break;

        case epoc::etel_get_tsy_name:
            get_tsy_name(ctx);
            break;

        case epoc::etel_phone_info_by_index:
            get_phone_info_by_index(ctx);
            break;

        case epoc::etel_load_phone_module:
            load_phone_module(ctx);
            break;

        default:
            LOG_ERROR("Unimplemented ETel server opcode {}", ctx->msg->function);
            break;
        }
    }
}