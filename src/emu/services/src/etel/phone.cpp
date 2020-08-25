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

#include <services/context.h>
#include <services/etel/phone.h>
#include <services/etel/subsess.h>
#include <utils/err.h>

#include <common/cvt.h>

namespace eka2l1 {
    etel_phone_subsession::etel_phone_subsession(etel_session *session, etel_phone *phone, bool oldarch)
        : etel_subsession(session)
        , phone_(phone)
        , oldarch_(oldarch) {
    }

    void etel_phone_subsession::get_status(service::ipc_context *ctx) {
        ctx->write_data_to_descriptor_argument<epoc::etel_phone_status>(0, phone_->status_);
        ctx->complete(epoc::error_none);
    }

    void etel_phone_subsession::init(service::ipc_context *ctx) {
        if (!phone_->init()) {
            ctx->complete(epoc::error_general);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    void etel_phone_subsession::enumerate_lines(service::ipc_context *ctx) {
        const std::uint32_t total_line = static_cast<std::uint32_t>(phone_->lines_.size());
        ctx->write_data_to_descriptor_argument<std::uint32_t>(0, total_line);
        ctx->complete(epoc::error_none);
    }

    void etel_phone_subsession::get_line_info(service::ipc_context *ctx) {
        struct line_info_package {
            std::int32_t index_;
            epoc::etel_line_info_from_phone info_;
        };

        std::optional<line_info_package> info_to_fill = ctx->get_argument_data_from_descriptor<line_info_package>(0);

        if (!info_to_fill) {
            ctx->complete(epoc::error_argument);
            return;
        }

        if ((info_to_fill->index_ < 0) || (info_to_fill->index_ >= phone_->lines_.size())) {
            ctx->complete(epoc::error_argument);
            return;
        }

        etel_line *line = phone_->lines_[info_to_fill->index_];

        info_to_fill->info_.name_.assign(nullptr, common::utf8_to_ucs2(line->name_));
        info_to_fill->info_.caps_ = line->caps_;
        info_to_fill->info_.sts_ = line->info_.sts_;

        ctx->write_data_to_descriptor_argument<line_info_package>(0, info_to_fill.value());
        ctx->complete(epoc::error_none);
    }

    void etel_phone_subsession::get_identity_caps(service::ipc_context *ctx) {
        LOG_TRACE("Get identity caps hardcoded");

        const std::uint32_t caps = epoc::etel_mobile_phone_identity_cap_get_manufacturer 
            | epoc::etel_mobile_phone_identity_cap_get_model 
            | epoc::etel_mobile_phone_identity_cap_get_revision
            | epoc::etel_mobile_phone_identity_cap_get_serialnumber
            | epoc::etel_mobile_phone_identity_cap_get_subscriber_id;

        ctx->write_data_to_descriptor_argument<std::uint32_t>(0, caps);
        ctx->complete(epoc::error_none);
    }

    void etel_phone_subsession::get_indicator_caps(service::ipc_context *ctx) {
        LOG_TRACE("Get indicator caps hardcoded");

        const std::uint32_t action_caps = epoc::etel_mobile_phone_indicator_cap_get;
        const std::uint32_t indicator_caps = epoc::etel_mobile_phone_indicator_charger_connected
            | epoc::etel_mobile_phone_indicator_network_avail
            | epoc::etel_mobile_phone_indicator_call_in_progress;

        ctx->write_data_to_descriptor_argument<std::uint32_t>(0, action_caps);
        ctx->write_data_to_descriptor_argument<std::uint32_t>(1, indicator_caps);

        ctx->complete(epoc::error_none);
    }

    void etel_phone_subsession::get_indicator(service::ipc_context *ctx) {
        LOG_TRACE("Get indicator hardcoded");
        const std::uint32_t indicator = epoc::etel_mobile_phone_indicator_network_avail;

        ctx->write_data_to_descriptor_argument<std::uint32_t>(0, indicator);
        ctx->complete(epoc::error_none);
    }

    void etel_phone_subsession::get_network_registration_status(eka2l1::service::ipc_context *ctx) {
        LOG_TRACE("Get network registration status hardcoded");
        const std::uint32_t network_registration_status = epoc::etel_mobile_phone_registered_on_home_network;

        ctx->write_data_to_descriptor_argument<std::uint32_t>(0, network_registration_status);
        ctx->complete(epoc::error_none);
    }

    void etel_phone_subsession::get_home_network(eka2l1::service::ipc_context *ctx) {
        LOG_TRACE("Get home network hardcoded");
        std::optional<epoc::etel_phone_network_info> network_info = ctx->get_argument_data_from_descriptor<epoc::etel_phone_network_info>(0);

        network_info->mode_ = phone_->network_info_.mode_;
        network_info->status_ = phone_->network_info_.status_;
        network_info->band_info_ = phone_->network_info_.band_info_;

        ctx->write_data_to_descriptor_argument<epoc::etel_phone_network_info>(0, network_info.value(), nullptr, true);
        ctx->complete(epoc::error_none);
    }

    static const std::u16string EXAMPLE_VALID_IMI_CODE = u"540806859904945";

    void etel_phone_subsession::get_phone_id(eka2l1::service::ipc_context *ctx) {
        LOG_TRACE("Get phone id hardcoded, random generated beforehand");

        phone_id_info_v1 id_info;
        id_info.the_id_.assign(nullptr, EXAMPLE_VALID_IMI_CODE);

        ctx->write_data_to_descriptor_argument<phone_id_info_v1>(0, id_info);
        ctx->complete(epoc::error_none);
    }

    void etel_phone_subsession::get_subscriber_id(eka2l1::service::ipc_context *ctx) {
        LOG_TRACE("Get subscriber id hardcoded, random generated beforehand");

        subscriber_id_info_v1 id_info;
        id_info.the_id_.assign(nullptr, EXAMPLE_VALID_IMI_CODE);

        ctx->write_data_to_descriptor_argument<subscriber_id_info_v1>(0, id_info);
        ctx->complete(epoc::error_none);
    }

    void etel_phone_subsession::get_current_network(eka2l1::service::ipc_context *ctx) {
        LOG_TRACE("Get current network hardcoded");  
        std::optional<epoc::etel_phone_network_info> network_info = 
            ctx->get_argument_data_from_descriptor<epoc::etel_phone_network_info>(0);
        epoc::etel_phone_location_area *phone_location_area = 
            reinterpret_cast<epoc::etel_phone_location_area *>(ctx->get_descriptor_argument_ptr(2));

        network_info->mode_ = phone_->network_info_.mode_;
        network_info->status_ = phone_->network_info_.status_;
        network_info->band_info_ = phone_->network_info_.band_info_;

        ctx->write_data_to_descriptor_argument<epoc::etel_phone_network_info>(0, network_info.value(), nullptr, true);
        ctx->complete(epoc::error_none);
    }

    void etel_phone_subsession::get_signal_strength(eka2l1::service::ipc_context *ctx) {
        std::int32_t *signal_strength_ptr = reinterpret_cast<std::int32_t *>(ctx->get_descriptor_argument_ptr(0));
        std::int32_t *bar_ptr = reinterpret_cast<std::int32_t *>(ctx->get_descriptor_argument_ptr(2));

        *signal_strength_ptr = 50;
        *bar_ptr = 7;
        ctx->complete(epoc::error_none);
    }

    void etel_phone_subsession::notify_network_registration_status_change(eka2l1::service::ipc_context *ctx) {
        network_registration_status_change_nof_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
    }

    void etel_phone_subsession::notify_signal_strength_change(eka2l1::service::ipc_context *ctx) {
        signal_strength_change_nof_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
    }

    void etel_phone_subsession::notify_current_network_change(eka2l1::service::ipc_context *ctx) {
        current_network_change_nof_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
    }

    void etel_phone_subsession::get_current_network_cancel(eka2l1::service::ipc_context *ctx) {
        LOG_TRACE("Get current network cancel stubbed");

        ctx->complete(epoc::error_none);
    }

    void etel_phone_subsession::get_network_registration_status_cancel(eka2l1::service::ipc_context *ctx) {
        LOG_TRACE("Get network registration status cancel stubbed");

        ctx->complete(epoc::error_none);
    }

    void etel_phone_subsession::dispatch(service::ipc_context *ctx) {
        if (oldarch_) {
            switch (ctx->msg->function) {
            case epoc::etel_old_phone_get_status:
                get_status(ctx);
                break;

            case epoc::etel_old_phone_get_line_info:
                get_line_info(ctx);
                break;

            default:
                LOG_ERROR("Unimplemented etel phone opcode {}", ctx->msg->function);
                break;
            }
        } else {
            switch (ctx->msg->function) {
            case epoc::etel_phone_init:
                init(ctx);
                break;

            case epoc::etel_phone_get_status:
                get_status(ctx);
                break;

            case epoc::etel_phone_enumerate_lines:
                enumerate_lines(ctx);
                break;

            case epoc::etel_phone_get_line_info:
                get_line_info(ctx);
                break;

            case epoc::etel_mobile_phone_get_identity_caps:
                get_identity_caps(ctx);
                break;

            case epoc::etel_mobile_phone_get_indicator_caps:
                get_indicator_caps(ctx);
                break;

            case epoc::etel_mobile_phone_get_indicator:
                get_indicator(ctx);
                break;

            case epoc::etel_mobile_phone_get_network_registration_status:
                get_network_registration_status(ctx);
                break;

            case epoc::etel_mobile_phone_get_signal_strength:
                get_signal_strength(ctx);
                break;

            case epoc::etel_mobile_phone_notify_network_registration_status_change:
                notify_network_registration_status_change(ctx);
                break;

            case epoc::etel_mobile_phone_notify_signal_strength_change:
                notify_signal_strength_change(ctx);
                break;

            case epoc::etel_mobile_phone_get_network_registration_status_cancel:
                get_network_registration_status_cancel(ctx);
                break;

            case epoc::etel_mobile_phone_get_home_network:
                get_home_network(ctx);
                break;

            case epoc::etel_mobile_phone_get_subscriber_id:
                get_subscriber_id(ctx);
                break;

            case epoc::etel_mobile_phone_get_phone_id:
                get_phone_id(ctx);
                break;

            case epoc::etel_mobile_phone_get_current_network:
                get_current_network(ctx);
                break;

            case epoc::etel_mobile_phone_notify_current_network_change:
                notify_current_network_change(ctx);
                break;

            case epoc::etel_mobile_phone_get_current_network_cancel:
                get_current_network_cancel(ctx);
                break;

            default:
                LOG_ERROR("Unimplemented etel phone opcode {}", ctx->msg->function);
                break;
            }
        }
    }

    etel_phone::etel_phone(const epoc::etel_phone_info &info)
        : info_(info) {
        // Initialize default mode
        status_.detect_ = epoc::etel_modem_detect_not_present;
        status_.mode_ = epoc::etel_phone_mode_idle;
    }

    etel_phone::~etel_phone() {
    }

    bool etel_phone::init() {
        status_.detect_ = epoc::etel_modem_detect_present;

        network_info_.mode_ = epoc::etel_mobile_phone_network_mode_gsm;
        network_info_.status_ = epoc::etel_mobile_phone_network_status_available;
        network_info_.band_info_ = epoc::etel_mobile_phone_band_unknown;
        return true;
    }
}