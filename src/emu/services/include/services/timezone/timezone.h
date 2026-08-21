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
 */

#pragma once

#include <services/framework.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace eka2l1::epoc::tz {
    /**
     * @brief Convert a Symbian TTime to a Unix timestamp, and back.
     *
     * TTime counts microseconds from the start of 1 January year 0; the emulator
     * already carries that distance as common::ad_epoc_dist_microsecs.
     */
    std::int64_t symbian_time_to_unix_seconds(const std::int64_t time);
    std::int64_t unix_seconds_to_symbian_time(const std::int64_t seconds);

    /**
     * @brief Serialise the host's transitions the way a client reads them back.
     *
     * The layout is the one CTzRules::InternalizeL expects: a header of start
     * year, end year, initial standard-time offset and rule count, all 16-bit,
     * then 44 bytes per rule as TTzRule::InternalizeL reads them.
     */
    std::vector<std::uint8_t> make_rules(const int requested_start_year, const int requested_end_year);
}

namespace eka2l1 {
    class timezone_server : public service::typical_server {
        std::string zone_name_;
        std::uint32_t zone_id_;

    public:
        explicit timezone_server(system *sys);

        void connect(service::ipc_context &context) override;
        void resolve_guest_zone();

        const std::string &zone_name() const {
            return zone_name_;
        }

        std::uint32_t zone_id() const {
            return zone_id_;
        }
    };

    class timezone_session : public service::typical_session {
        std::vector<std::uint8_t> pending_rules_;
        std::unique_ptr<service::ipc_context> change_notification_;

        void get_local_id(service::ipc_context *ctx);
        void convert_local(service::ipc_context *ctx);
        void convert_foreign(service::ipc_context *ctx);
        void get_rules_size(service::ipc_context *ctx, bool foreign);
        void get_rules(service::ipc_context *ctx);
        void get_offsets(service::ipc_context *ctx);
        void is_daylight_saving(service::ipc_context *ctx);
        void register_change_notifier(service::ipc_context *ctx);
        void cancel_change_notifier(service::ipc_context *ctx);

    public:
        explicit timezone_session(service::typical_server *server, kernel::uid session_id,
            epoc::version client_version);
        ~timezone_session() override;

        void fetch(service::ipc_context *ctx) override;
    };
}
