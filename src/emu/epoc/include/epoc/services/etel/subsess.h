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

#include <epoc/services/etel/common.h>
#include <epoc/utils/reqsts.h>

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

        std::string name_;
        std::uint32_t id_;

        etel_session *session_;

    public:
        explicit etel_subsession(etel_session *session);

        virtual void dispatch(service::ipc_context *ctx) = 0;
        virtual etel_subsession_type type() const = 0;
    };

    struct etel_phone_subsession : public etel_subsession {
        etel_phone *phone_;

    protected:
        void get_status(service::ipc_context *ctx);
        void init(service::ipc_context *ctx);
        void enumerate_lines(service::ipc_context *ctx);
        void get_line_info(service::ipc_context *ctx);
        void get_indicator_caps(service::ipc_context *ctx);
        void get_indicator(service::ipc_context *ctx);

    public:
        explicit etel_phone_subsession(etel_session *session, etel_phone *phone);

        void dispatch(service::ipc_context *ctx) override;

        etel_subsession_type type() const override {
            return etel_subsession_type_phone;
        }
    };

    struct etel_line_subsession : public etel_subsession {
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
        explicit etel_line_subsession(etel_session *session, etel_line *line);

        void dispatch(service::ipc_context *ctx) override;

        etel_subsession_type type() const override {
            return etel_subsession_type_line;
        }
    };
};
