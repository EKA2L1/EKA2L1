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

#include <services/timezone/timezone.h>

#include <common/log.h>
#include <common/time.h>
#include <system/epoc.h>
#include <utils/err.h>
#include <vfs/vfs.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

namespace eka2l1 {
    namespace {
        enum timezone_opcode {
            opcode_get_local_id = 0,
            opcode_convert_local_zone_time = 1,
            opcode_convert_foreign_zone_time = 2,
            opcode_get_local_rules_size = 3,
            opcode_get_local_rules = 6,
            opcode_get_foreign_rules_size = 5,
            opcode_get_foreign_rules = 8,
            opcode_register_time_change_notifier = 10,
            opcode_cancel_request_for_notice = 11,
            opcode_get_offsets_for_ids = 14,
            opcode_daylight_saving_state = 15,
            opcode_auto_update_setting = 16
        };

        constexpr std::int64_t seconds_per_day = 86400;
        constexpr std::int64_t microseconds_per_second = 1000000;
        constexpr std::int64_t symbian_epoch_microseconds =
            static_cast<std::int64_t>(common::ad_epoc_dist_microsecs);

        template <typename T>
        bool read_little_endian(const std::vector<std::uint8_t> &data, const std::size_t offset, T &value) {
            static_assert(std::is_integral_v<T>);
            if (offset > data.size() || data.size() - offset < sizeof(T)) {
                return false;
            }

            using unsigned_type = std::make_unsigned_t<T>;
            unsigned_type result = 0;
            for (std::size_t i = 0; i < sizeof(T); ++i) {
                result |= static_cast<unsigned_type>(data[offset + i]) << (i * 8);
            }
            value = static_cast<T>(result);
            return true;
        }

        template <typename T>
        void append_little_endian(std::vector<std::uint8_t> &data, const T value) {
            static_assert(std::is_integral_v<T>);
            using unsigned_type = std::make_unsigned_t<T>;
            const unsigned_type converted = static_cast<unsigned_type>(value);
            for (std::size_t i = 0; i < sizeof(T); ++i) {
                data.push_back(static_cast<std::uint8_t>((converted >> (i * 8)) & 0xFF));
            }
        }

        bool read_database_string(const std::vector<std::uint8_t> &database, const std::uint32_t string_table,
            const std::uint16_t reference, std::string &result) {
            const std::size_t position = static_cast<std::size_t>(string_table) + reference;
            if (position >= database.size()) {
                return false;
            }

            const std::size_t length = database[position];
            if (database.size() - position - 1 < length) {
                return false;
            }

            result.assign(reinterpret_cast<const char *>(database.data() + position + 1), length);
            return true;
        }

        bool read_zone_record(const std::vector<std::uint8_t> &database, const std::uint32_t string_table,
            const std::uint32_t zone_data, const std::uint16_t record_offset, std::uint32_t &identifier,
            std::string &name) {
            const std::size_t position = static_cast<std::size_t>(zone_data) + record_offset;
            std::uint16_t numeric_id = 0;
            std::uint16_t location_ref = 0;
            std::uint16_t region_ref = 0;
            if (!read_little_endian(database, position, numeric_id)
                || !read_little_endian(database, position + 2, location_ref)
                || !read_little_endian(database, position + 4, region_ref)) {
                return false;
            }

            std::string location;
            std::string region;
            if (!read_database_string(database, string_table, location_ref, location)
                || !read_database_string(database, string_table, region_ref, region)) {
                return false;
            }

            identifier = numeric_id;
            name = region.empty() ? location : region + "/" + location;
            return true;
        }

        std::pair<std::uint32_t, std::string> resolve_zone_from_database(io_system *io,
            const std::string &host_zone) {
            symfile file = io->open_file(u"z:\\private\\1020383e\\tzdb.dbz", READ_MODE | BIN_MODE);
            if (!file || !file->valid() || file->size() < 52
                || file->size() > std::numeric_limits<std::uint32_t>::max()) {
                return { 0, host_zone };
            }

            std::vector<std::uint8_t> database(static_cast<std::size_t>(file->size()));
            if (file->read_file(database.data(), static_cast<std::uint32_t>(database.size()), 1)
                != database.size()) {
                return { 0, host_zone };
            }

            std::uint32_t string_table = 0;
            std::uint32_t zone_data = 0;
            std::uint32_t zone_table = 0;
            std::uint32_t links_table = 0;
            std::uint32_t default_zone_offset = 0;
            if (!read_little_endian(database, 8, string_table)
                || !read_little_endian(database, 20, zone_data)
                || !read_little_endian(database, 24, zone_table)
                || !read_little_endian(database, 28, links_table)
                || !read_little_endian(database, 48, default_zone_offset)) {
                return { 0, host_zone };
            }

            std::uint32_t default_id = 0;
            std::string default_name;
            read_zone_record(database, string_table, zone_data,
                static_cast<std::uint16_t>(default_zone_offset), default_id, default_name);

            std::uint16_t zone_count = 0;
            if (read_little_endian(database, zone_table, zone_count)) {
                for (std::uint16_t index = 0; index < zone_count; ++index) {
                    std::uint16_t record_offset = 0;
                    if (!read_little_endian(database, zone_table + 2 + index * 2, record_offset)) {
                        break;
                    }

                    std::uint32_t numeric_id = 0;
                    std::string name;
                    if (read_zone_record(database, string_table, zone_data, record_offset, numeric_id, name)
                        && name == host_zone) {
                        return { numeric_id, name };
                    }
                }
            }

            std::uint16_t link_count = 0;
            if (read_little_endian(database, links_table, link_count)) {
                for (std::uint16_t index = 0; index < link_count; ++index) {
                    std::uint16_t name_reference = 0;
                    std::uint16_t record_offset = 0;
                    const std::size_t entry = static_cast<std::size_t>(links_table) + 2 + index * 4;
                    if (!read_little_endian(database, entry, name_reference)
                        || !read_little_endian(database, entry + 2, record_offset)) {
                        break;
                    }

                    std::string link_name;
                    if (read_database_string(database, string_table, name_reference, link_name)
                        && link_name == host_zone) {
                        std::uint32_t numeric_id = 0;
                        std::string canonical_name;
                        if (read_zone_record(database, string_table, zone_data, record_offset,
                                numeric_id, canonical_name)) {
                            return { numeric_id, host_zone };
                        }
                    }
                }
            }

            if (default_id != 0) {
                LOG_WARN(SERVICE_TIMEZONE,
                    "Host time zone '{}' is absent from the guest database; using guest default '{}' ({})",
                    host_zone, default_name, default_id);
                return { default_id, host_zone };
            }

            return { 0, host_zone };
        }

        // Howard Hinnant's public-domain civil calendar algorithms.
        constexpr std::int64_t days_from_civil(int year, const unsigned month, const unsigned day) {
            year -= month <= 2;
            const int era = (year >= 0 ? year : year - 399) / 400;
            const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
            const unsigned day_of_year =
                (153 * (month + (month > 2 ? static_cast<unsigned>(-3) : 9)) + 2) / 5 + day - 1;
            const unsigned day_of_era =
                year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
            return era * 146097 + static_cast<int>(day_of_era) - 719468;
        }

        struct civil_date {
            int year;
            unsigned month;
            unsigned day;
        };

        constexpr civil_date civil_from_days(std::int64_t days) {
            days += 719468;
            const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
            const unsigned day_of_era = static_cast<unsigned>(days - era * 146097);
            const unsigned year_of_era =
                (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
            int year = static_cast<int>(year_of_era) + static_cast<int>(era) * 400;
            const unsigned day_of_year =
                day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
            const unsigned month_prime = (5 * day_of_year + 2) / 153;
            const unsigned day = day_of_year - (153 * month_prime + 2) / 5 + 1;
            const unsigned month = month_prime + (month_prime < 10 ? 3 : static_cast<unsigned>(-9));
            year += month <= 2;
            return { year, month, day };
        }

        constexpr std::int64_t unix_seconds_at_year_start(const int year) {
            return days_from_civil(year, 1, 1) * seconds_per_day;
        }

        int year_from_unix_seconds(const std::int64_t seconds) {
            std::int64_t days = seconds / seconds_per_day;
            if (seconds < 0 && seconds % seconds_per_day != 0) {
                --days;
            }
            return civil_from_days(days).year;
        }

        struct transition {
            std::int64_t unix_seconds;
            int old_offset_seconds;
            int new_offset_seconds;
        };

        std::vector<transition> find_transitions(const std::int64_t start, const std::int64_t end) {
            std::vector<transition> transitions;
            constexpr std::int64_t sample_interval = 7 * seconds_per_day;

            std::int64_t previous_time = start;
            int previous_offset = common::get_local_time_zone_info(start).offset_seconds;
            for (std::int64_t sample_time = std::min(start + sample_interval, end);
                 previous_time < end;
                 sample_time = std::min(sample_time + sample_interval, end)) {
                const int sample_offset = common::get_local_time_zone_info(sample_time).offset_seconds;
                if (sample_offset != previous_offset) {
                    std::int64_t low = previous_time + 1;
                    std::int64_t high = sample_time;
                    while (low < high) {
                        const std::int64_t middle = low + (high - low) / 2;
                        if (common::get_local_time_zone_info(middle).offset_seconds == previous_offset) {
                            low = middle + 1;
                        } else {
                            high = middle;
                        }
                    }

                    const int new_offset = common::get_local_time_zone_info(low).offset_seconds;
                    transitions.push_back({ low, previous_offset, new_offset });
                    previous_offset = new_offset;
                }

                previous_time = sample_time;
                if (previous_time == end) {
                    break;
                }
                sample_time = std::min(sample_time, end);
            }

            return transitions;
        }

        bool read_descriptor_integer(service::ipc_context *ctx, const int slot, std::int64_t &value) {
            const auto data = ctx->get_argument_data_from_descriptor<std::int64_t>(slot);
            if (!data) {
                return false;
            }
            value = *data;
            return true;
        }

        bool read_time_reference(service::ipc_context *ctx, const int slot, std::int32_t &reference) {
            const auto data = ctx->get_argument_data_from_descriptor<std::int32_t>(slot);
            if (!data) {
                return false;
            }
            reference = *data;
            return reference >= 0 && reference <= 2;
        }

        std::int64_t convert_time(const std::int64_t symbian_time, const std::int32_t reference) {
            std::int64_t unix_seconds = epoc::tz::symbian_time_to_unix_seconds(symbian_time);
            if (reference == 0) {
                unix_seconds += common::get_local_time_zone_info(unix_seconds).offset_seconds;
            } else {
                const std::int64_t local_scalar = unix_seconds;
                std::int64_t candidate = local_scalar
                    - common::get_local_time_zone_info(local_scalar).offset_seconds;
                for (int iteration = 0; iteration < 4; ++iteration) {
                    const std::int64_t next = local_scalar
                        - common::get_local_time_zone_info(candidate).offset_seconds;
                    if (next == candidate) {
                        break;
                    }
                    candidate = next;
                }
                unix_seconds = candidate;
            }

            return epoc::tz::unix_seconds_to_symbian_time(unix_seconds);
        }
    }

    namespace epoc::tz {
        std::int64_t symbian_time_to_unix_seconds(const std::int64_t time) {
            return (time - symbian_epoch_microseconds) / microseconds_per_second;
        }

        std::int64_t unix_seconds_to_symbian_time(const std::int64_t seconds) {
            return seconds * microseconds_per_second + symbian_epoch_microseconds;
        }

        std::vector<std::uint8_t> make_rules(const int requested_start_year, const int requested_end_year) {
            const int start_year = std::clamp(requested_start_year, 0, 9998);
            const int end_year = std::clamp(std::max(requested_end_year, start_year), start_year, 9998);
            const std::int64_t requested_start = unix_seconds_at_year_start(start_year);
            const int initial_offset = common::get_local_time_zone_info(requested_start).offset_seconds / 60;

            // Some clients ask for the full Symbian range (year 0 through
            // 9999). Host tzdata has concrete historical transitions only
            // around the Unix era, and walking ten millennia would block the
            // server. Cover the useful tzdata window while retaining the
            // requested range in the serialized CTzRules header.
            const int transition_start_year = std::max(start_year, 1900);
            const int transition_end_year = std::min(end_year, 2100);
            std::vector<transition> transitions;
            if (transition_start_year <= transition_end_year) {
                transitions = find_transitions(unix_seconds_at_year_start(transition_start_year),
                    unix_seconds_at_year_start(transition_end_year + 1));
            }
            if (transitions.size() > static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max())) {
                transitions.resize(std::numeric_limits<std::int16_t>::max());
            }

            std::vector<std::uint8_t> result;
            result.reserve(8 + transitions.size() * 44);
            append_little_endian(result, static_cast<std::int16_t>(start_year));
            append_little_endian(result, static_cast<std::int16_t>(end_year));
            append_little_endian(result, static_cast<std::int16_t>(initial_offset));
            append_little_endian(result, static_cast<std::int16_t>(transitions.size()));

            for (const transition &item : transitions) {
                std::int64_t day_number = item.unix_seconds / seconds_per_day;
                std::int64_t seconds_of_day = item.unix_seconds % seconds_per_day;
                if (seconds_of_day < 0) {
                    seconds_of_day += seconds_per_day;
                    --day_number;
                }
                const civil_date date = civil_from_days(day_number);
                const std::int64_t rule_start = unix_seconds_at_year_start(date.year);
                const std::int64_t rule_end = unix_seconds_at_year_start(date.year + 1) - 1;

                append_little_endian(result, unix_seconds_to_symbian_time(rule_start));
                append_little_endian(result, static_cast<std::int32_t>(0)); // UTC
                append_little_endian(result, unix_seconds_to_symbian_time(rule_end));
                append_little_endian(result, static_cast<std::int32_t>(0)); // UTC
                append_little_endian(result, static_cast<std::int16_t>(item.old_offset_seconds / 60));
                append_little_endian(result, static_cast<std::int16_t>(item.new_offset_seconds / 60));
                append_little_endian(result, static_cast<std::int32_t>(date.month - 1));
                append_little_endian(result, static_cast<std::int32_t>(0)); // fixed date
                append_little_endian(result, static_cast<std::uint8_t>(date.day - 1));
                append_little_endian(result, static_cast<std::uint8_t>(0));
                append_little_endian(result, static_cast<std::int32_t>(0)); // UTC
                append_little_endian(result, static_cast<std::uint16_t>(seconds_of_day / 60));
            }

            return result;
        }
    }

    timezone_server::timezone_server(system *sys)
        : service::typical_server(sys, "!TzServer")
        , zone_name_(common::get_current_time_zone_name())
        , zone_id_(0) {
    }

    void timezone_server::resolve_guest_zone() {
        const std::string host_zone = common::get_current_time_zone_name();
        std::tie(zone_id_, zone_name_) = resolve_zone_from_database(get_system()->get_io_system(), host_zone);
        LOG_INFO(SERVICE_TIMEZONE, "Using host time zone '{}' (guest ID {}, current UTC offset {:+d} minutes)",
            zone_name_, zone_id_, common::get_current_utc_offset() / 60);
    }

    void timezone_server::connect(service::ipc_context &context) {
        // The selected ROM drive is mounted after HLE services are created.
        // Resolve against it when the first guest client actually connects.
        if (zone_id_ == 0 || zone_name_ != common::get_current_time_zone_name()) {
            resolve_guest_zone();
        }
        create_session<timezone_session>(&context);
        context.complete(epoc::error_none);
    }

    timezone_session::timezone_session(service::typical_server *server, const kernel::uid session_id,
        const epoc::version client_version)
        : service::typical_session(server, session_id, client_version) {
    }

    timezone_session::~timezone_session() {
        if (!change_notification_ || !change_notification_->msg) {
            return;
        }

        kernel_system *kernel = server<timezone_server>()->get_kernel_object_owner();
        if (kernel->is_thread_alive(change_notification_->msg->own_thr)) {
            change_notification_->complete(epoc::error_cancel);
        }
        change_notification_.reset();
    }

    void timezone_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case opcode_get_local_id:
            get_local_id(ctx);
            break;
        case opcode_convert_local_zone_time:
            convert_local(ctx);
            break;
        case opcode_convert_foreign_zone_time:
            convert_foreign(ctx);
            break;
        case opcode_get_local_rules_size:
            get_rules_size(ctx, false);
            break;
        case opcode_get_foreign_rules_size:
            get_rules_size(ctx, true);
            break;
        case opcode_get_local_rules:
        case opcode_get_foreign_rules:
            get_rules(ctx);
            break;
        case opcode_register_time_change_notifier:
            register_change_notifier(ctx);
            break;
        case opcode_cancel_request_for_notice:
            cancel_change_notifier(ctx);
            break;
        case opcode_get_offsets_for_ids:
            get_offsets(ctx);
            break;
        case opcode_daylight_saving_state:
            is_daylight_saving(ctx);
            break;
        case opcode_auto_update_setting: {
            const std::int32_t enabled = 1;
            if (!ctx->write_data_to_descriptor_argument(0, enabled)) {
                ctx->complete(epoc::error_bad_descriptor);
                return;
            }
            ctx->complete(epoc::error_none);
            break;
        }
        case 1001: // Enable auto-update. Host settings are always authoritative.
            ctx->complete(epoc::error_none);
            break;
        default:
            LOG_WARN(SERVICE_TIMEZONE, "Unimplemented TZSERVER opcode {}", ctx->msg->function);
            ctx->complete(epoc::error_not_supported);
            break;
        }
    }

    void timezone_session::get_local_id(service::ipc_context *ctx) {
        const timezone_server *timezone = server<timezone_server>();
        std::vector<std::uint8_t> serialized;
        serialized.reserve(8 + timezone->zone_name().size());
        append_little_endian(serialized, timezone->zone_id());
        append_little_endian(serialized, static_cast<std::int32_t>(timezone->zone_name().size()));
        serialized.insert(serialized.end(), timezone->zone_name().begin(), timezone->zone_name().end());

        if (!ctx->write_data_to_descriptor_argument(0, serialized.data(),
                static_cast<std::uint32_t>(serialized.size()))) {
            ctx->complete(epoc::error_bad_descriptor);
            return;
        }
        ctx->complete(epoc::error_none);
    }

    void timezone_session::convert_local(service::ipc_context *ctx) {
        std::int64_t input = 0;
        std::int32_t reference = 0;
        if (!read_descriptor_integer(ctx, 0, input) || !read_time_reference(ctx, 1, reference)) {
            ctx->complete(epoc::error_bad_descriptor);
            return;
        }

        const std::int64_t output = convert_time(input, reference);
        if (!ctx->write_data_to_descriptor_argument(2, output)) {
            ctx->complete(epoc::error_bad_descriptor);
            return;
        }
        ctx->complete(epoc::error_none);
    }

    void timezone_session::convert_foreign(service::ipc_context *ctx) {
        std::int64_t input = 0;
        std::int32_t reference = 0;
        const std::uint8_t *zone = ctx->get_descriptor_argument_ptr(2);
        const std::size_t zone_size = ctx->get_argument_data_size(2);
        std::uint32_t numeric_id = 0;
        if (!read_descriptor_integer(ctx, 0, input) || !read_time_reference(ctx, 1, reference)
            || !zone || zone_size < sizeof(numeric_id)) {
            ctx->complete(epoc::error_bad_descriptor);
            return;
        }
        std::memcpy(&numeric_id, zone, sizeof(numeric_id));
        if (numeric_id != server<timezone_server>()->zone_id()) {
            ctx->complete(epoc::error_not_supported);
            return;
        }

        const std::int64_t output = convert_time(input, reference);
        if (!ctx->write_data_to_descriptor_argument(3, output)) {
            ctx->complete(epoc::error_bad_descriptor);
            return;
        }
        ctx->complete(epoc::error_none);
    }

    void timezone_session::get_rules_size(service::ipc_context *ctx, const bool foreign) {
        std::int64_t start = 0;
        std::int64_t end = 0;
        if (!read_descriptor_integer(ctx, 0, start) || !read_descriptor_integer(ctx, 1, end)) {
            ctx->complete(epoc::error_bad_descriptor);
            return;
        }

        if (foreign) {
            const std::uint8_t *zone = ctx->get_descriptor_argument_ptr(2);
            const std::size_t zone_size = ctx->get_argument_data_size(2);
            std::uint32_t numeric_id = 0;
            if (!zone || zone_size < sizeof(numeric_id) + sizeof(std::int32_t) + 1) {
                ctx->complete(epoc::error_bad_descriptor);
                return;
            }
            std::memcpy(&numeric_id, zone, sizeof(numeric_id));
            if (numeric_id != server<timezone_server>()->zone_id()) {
                ctx->complete(epoc::error_not_supported);
                return;
            }
        } else {
            std::int32_t reference = 0;
            if (!read_time_reference(ctx, 2, reference)) {
                ctx->complete(epoc::error_bad_descriptor);
                return;
            }
        }

        const int start_year = year_from_unix_seconds(epoc::tz::symbian_time_to_unix_seconds(start));
        const int end_year = year_from_unix_seconds(epoc::tz::symbian_time_to_unix_seconds(end));
        pending_rules_ = epoc::tz::make_rules(start_year, end_year);
        const std::int32_t size = static_cast<std::int32_t>(pending_rules_.size());
        if (!ctx->write_data_to_descriptor_argument(3, size)) {
            ctx->complete(epoc::error_bad_descriptor);
            return;
        }
        ctx->complete(epoc::error_none);
    }

    void timezone_session::get_rules(service::ipc_context *ctx) {
        const auto requested_size = ctx->get_argument_data_from_descriptor<std::int32_t>(0);
        if (!requested_size || *requested_size <= 0 || pending_rules_.empty()
            || static_cast<std::size_t>(*requested_size) != pending_rules_.size()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        if (!ctx->write_data_to_descriptor_argument(1, pending_rules_.data(),
                static_cast<std::uint32_t>(pending_rules_.size()))) {
            ctx->complete(epoc::error_bad_descriptor);
            return;
        }
        ctx->complete(epoc::error_none);
    }

    void timezone_session::get_offsets(service::ipc_context *ctx) {
        const auto byte_size = ctx->get_argument_value<std::int32_t>(0);
        std::uint8_t *buffer = ctx->get_descriptor_argument_ptr(1);
        const std::size_t buffer_size = ctx->get_argument_data_size(1);
        if (!byte_size || *byte_size < 4 || !buffer || buffer_size < 4
            || static_cast<std::size_t>(*byte_size) > buffer_size) {
            ctx->complete(epoc::error_bad_descriptor);
            return;
        }

        std::int32_t count = 0;
        std::memcpy(&count, buffer, sizeof(count));
        if (count < 0 || static_cast<std::size_t>(count) > (buffer_size - 4) / 4) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const std::int32_t current_offset = common::get_current_utc_offset() / 60;
        for (std::int32_t index = 0; index < count; ++index) {
            std::uint32_t identifier = 0;
            std::memcpy(&identifier, buffer + 4 + index * 4, sizeof(identifier));
            const std::int32_t offset =
                identifier == server<timezone_server>()->zone_id() ? current_offset : 0;
            std::memcpy(buffer + 4 + index * 4, &offset, sizeof(offset));
        }
        ctx->complete(epoc::error_none);
    }

    void timezone_session::is_daylight_saving(service::ipc_context *ctx) {
        std::int64_t time = static_cast<std::int64_t>(common::get_current_utc_time_in_microseconds_since_0ad());
        const auto supplied_time = ctx->get_argument_data_from_descriptor<std::int64_t>(1);
        if (supplied_time && *supplied_time != 0) {
            time = *supplied_time;
        }

        const std::int32_t result = common::get_local_time_zone_info(
            epoc::tz::symbian_time_to_unix_seconds(time)).daylight_saving
            ? 1
            : 0;
        if (!ctx->write_data_to_descriptor_argument(2, result)) {
            ctx->complete(epoc::error_bad_descriptor);
            return;
        }
        ctx->complete(epoc::error_none);
    }

    void timezone_session::register_change_notifier(service::ipc_context *ctx) {
        if (change_notification_) {
            ctx->complete(epoc::error_in_use);
            return;
        }
        change_notification_ = ctx->move_to_new();
    }

    void timezone_session::cancel_change_notifier(service::ipc_context *ctx) {
        if (change_notification_) {
            kernel_system *kernel = server<timezone_server>()->get_kernel_object_owner();
            if (change_notification_->msg
                && kernel->is_thread_alive(change_notification_->msg->own_thr)) {
                change_notification_->complete(epoc::error_cancel);
            }
            change_notification_.reset();
        }
        ctx->complete(epoc::error_none);
    }
}
