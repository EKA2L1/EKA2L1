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

#pragma once

#include <services/etel/common.h>
#include <utils/reqsts.h>

#include <cstdint>
#include <string>

namespace eka2l1 {
    namespace service {
        struct ipc_context;
    };

    struct etel_phone;
    struct etel_line;
    struct etel_session;

    enum etel_subsession_type {
        etel_subsession_type_phone = 0,
        etel_subsession_type_line = 1
    };

    struct etel_subsession {
    protected:
        friend struct etel_session;

        etel_legacy_level legacy_level_;

        std::string name_;
        std::uint32_t id_;

        etel_session *session_;

    public:
        explicit etel_subsession(etel_session *session, const etel_legacy_level lvl);

        virtual void dispatch(service::ipc_context *ctx) = 0;
        virtual etel_subsession_type type() const = 0;
        virtual ~etel_subsession() = default;
    };

    struct etel_phone_subsession : public etel_subsession {
        etel_phone *phone_;
        epoc::notify_info network_registration_status_change_nof_;
        epoc::notify_info signal_strength_change_nof_;
        epoc::notify_info current_network_change_nof_;
        epoc::notify_info indicator_change_nof_;
        epoc::notify_info battery_info_change_nof_;

    protected:
        void get_status(service::ipc_context *ctx);
        void init(service::ipc_context *ctx);
        void enumerate_lines(service::ipc_context *ctx);
        void get_line_info(service::ipc_context *ctx);
        void get_identity_caps(service::ipc_context *ctx);
        void get_indicator_caps(service::ipc_context *ctx);
        void get_indicator(service::ipc_context *ctx);
        void get_network_caps(service::ipc_context *ctx);
        void get_network_registration_status(eka2l1::service::ipc_context *ctx);
        void get_home_network(eka2l1::service::ipc_context *ctx);
        void get_phone_id(eka2l1::service::ipc_context *ctx);
        void get_subscriber_id(eka2l1::service::ipc_context *ctx);
        void get_subscriber_id_old(eka2l1::service::ipc_context *ctx);
        void get_current_network(eka2l1::service::ipc_context *ctx);
        void get_signal_strength(eka2l1::service::ipc_context *ctx);
        void get_battery_info(eka2l1::service::ipc_context *ctx);
        void notify_network_registration_status_change(eka2l1::service::ipc_context *ctx);
        void get_network_registration_status_cancel(eka2l1::service::ipc_context *ctx);
        void notify_signal_strength_change(eka2l1::service::ipc_context *ctx);
        void notify_current_network_change(eka2l1::service::ipc_context *ctx);
        void notify_indicator_change(eka2l1::service::ipc_context *ctx);
        void cancel_indicator_change(eka2l1::service::ipc_context *ctx);
        void get_current_network_cancel(eka2l1::service::ipc_context *ctx);
        void notify_battery_info(eka2l1::service::ipc_context *ctx);
        void notify_battery_info_cancel(eka2l1::service::ipc_context *ctx);

        void get_current_network_info_old(eka2l1::service::ipc_context *ctx);

    public:
        explicit etel_phone_subsession(etel_session *session, etel_phone *phone, const etel_legacy_level lvl);

        void dispatch(service::ipc_context *ctx) override;

        etel_subsession_type type() const override {
            return etel_subsession_type_phone;
        }
    };

    struct etel_line_subsession : public etel_subsession {
        bool oldarch_;
        etel_line *line_;
        epoc::notify_info status_change_nof_;
        epoc::notify_info incoming_call_nof_;

    protected:
        void get_status(service::ipc_context *ctx);
        void notify_status_change(service::ipc_context *ctx);
        void cancel_notify_status_change(service::ipc_context *ctx);
        void notify_incoming_call(service::ipc_context *ctx);
        void cancel_notify_incoming_call(service::ipc_context *ctx);

    public:
        explicit etel_line_subsession(etel_session *session, etel_line *line, const etel_legacy_level lvl);

        void dispatch(service::ipc_context *ctx) override;

        etel_subsession_type type() const override {
            return etel_subsession_type_line;
        }
    };
};
