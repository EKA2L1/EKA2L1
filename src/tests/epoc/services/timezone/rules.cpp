/*
 * Copyright (c) 2026 EKA2L1 Team.
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <catch2/catch.hpp>

#include <common/time.h>
#include <services/timezone/timezone.h>

#include <cstdlib>
#include <ctime>

using namespace eka2l1;

namespace {
    // The client decodes what the server sends with CTzRules::InternalizeL, so the
    // buffer has to match that reader exactly. From Symbian's own
    // tzservices/tzserver/Client/Source/vtzrules.cpp:
    //
    //   CTzRules::InternalizeL: ReadInt16L start year, ReadInt16L end year,
    //                           ReadInt16L initial std offset, ReadInt16L count
    //   TTzRule::InternalizeL:  TInt64 from, TInt32 from reference,
    //                           TInt64 to,   TInt32 to reference,
    //                           TInt16 old offset, TInt16 new offset,
    //                           TInt32 month, TInt32 day rule,
    //                           TUint8 day of month, TUint8 day of week,
    //                           TInt32 time reference, TUint16 time of change
    //
    // which is 8 bytes of header and 44 per rule, at these offsets inside one:
    constexpr std::size_t RULES_HEADER_SIZE = 8;
    constexpr std::size_t RULE_SIZE = 44;
    constexpr std::size_t RULE_FROM = 0;
    constexpr std::size_t RULE_FROM_REFERENCE = 8;
    constexpr std::size_t RULE_TO = 12;
    constexpr std::size_t RULE_TO_REFERENCE = 20;
    constexpr std::size_t RULE_OLD_OFFSET = 24;
    constexpr std::size_t RULE_NEW_OFFSET = 26;
    constexpr std::size_t RULE_MONTH = 28;
    constexpr std::size_t RULE_DAY_RULE = 32;
    constexpr std::size_t RULE_DAY_OF_MONTH = 36;
    constexpr std::size_t RULE_TIME_REFERENCE = 38;
    constexpr std::size_t RULE_TIME_OF_CHANGE = 42;

    template <typename T>
    T read_at(const std::vector<std::uint8_t> &data, const std::size_t offset) {
        T value = 0;
        for (std::size_t i = 0; i < sizeof(T); i++) {
            value |= static_cast<T>(static_cast<std::make_unsigned_t<T>>(data[offset + i])
                << (8 * i));
        }

        return value;
    }

    void select_time_zone(const char *name) {
#ifdef _WIN32
        _putenv_s("TZ", name);
        _tzset();
#else
        if (name[0] == '\0') {
            unsetenv("TZ");
        } else {
            setenv("TZ", name, 1);
        }

        tzset();
#endif
    }

    // The tests pin the host zone, since the rules are the host's own transitions.
    struct scoped_time_zone {
        std::string previous;

        explicit scoped_time_zone(const char *name) {
            const char *current = std::getenv("TZ");
            if (current) {
                previous = current;
            }

            select_time_zone(name);
        }

        ~scoped_time_zone() {
            select_time_zone(previous.c_str());
        }
    };
}

TEST_CASE("symbian_time_matches_the_ad_epoch", "timezone") {
    // TTime counts microseconds from the start of 1 January year 0, which is
    // 62168256000 seconds before the Unix epoch (common::ad_epoc_dist_microsecs).
    REQUIRE(epoc::tz::unix_seconds_to_symbian_time(0)
        == static_cast<std::int64_t>(common::ad_epoc_dist_microsecs));
    REQUIRE(epoc::tz::symbian_time_to_unix_seconds(
                static_cast<std::int64_t>(common::ad_epoc_dist_microsecs))
        == 0);

    // A time before the Unix epoch is still after the Symbian one, so it stays
    // positive: 1960-01-01 is 315619200 seconds before 1970.
    const std::int64_t nineteen_sixty = -315619200;
    REQUIRE(epoc::tz::unix_seconds_to_symbian_time(nineteen_sixty) > 0);
    REQUIRE(epoc::tz::symbian_time_to_unix_seconds(
                epoc::tz::unix_seconds_to_symbian_time(nineteen_sixty))
        == nineteen_sixty);
}

TEST_CASE("rules_for_a_zone_without_transitions_are_a_bare_header", "timezone") {
    scoped_time_zone zone("UTC");

    const std::vector<std::uint8_t> rules = epoc::tz::make_rules(2000, 2020);
    REQUIRE(rules.size() == RULES_HEADER_SIZE);

    // The years the client asked for come back untouched, so a client that
    // requested a range knows the answer covers it.
    REQUIRE(read_at<std::int16_t>(rules, 0) == 2000);
    REQUIRE(read_at<std::int16_t>(rules, 2) == 2020);
    // UTC never leaves its offset, so there is no rule to describe.
    REQUIRE(read_at<std::int16_t>(rules, 4) == 0);
    REQUIRE(read_at<std::int16_t>(rules, 6) == 0);
}

// Zone names only mean something to a CRT that carries a tzdata: the Windows one
// takes a POSIX TZ string and applies US daylight-saving rules to it, so there is no
// way to ask it for the transitions of a named zone.
#ifndef _WIN32
TEST_CASE("rules_carry_one_entry_per_transition", "timezone") {
    scoped_time_zone zone("Europe/London");

    const std::vector<std::uint8_t> rules = epoc::tz::make_rules(2000, 2019);

    // London changes twice a year, and both changes are inside the window.
    const std::int16_t count = read_at<std::int16_t>(rules, 6);
    REQUIRE(count == 2 * (2019 - 2000 + 1));
    REQUIRE(rules.size() == RULES_HEADER_SIZE + static_cast<std::size_t>(count) * RULE_SIZE);

    // London is on UTC in January, so that is the offset the rules start from.
    REQUIRE(read_at<std::int16_t>(rules, 4) == 0);

    for (std::int16_t index = 0; index < count; index++) {
        const std::size_t rule = RULES_HEADER_SIZE + static_cast<std::size_t>(index) * RULE_SIZE;

        // TTzRule keeps the window the rule applies to as a TTime pair. Both ends
        // have to land inside the year the transition belongs to.
        const std::int64_t from = epoc::tz::symbian_time_to_unix_seconds(read_at<std::int64_t>(rules, rule + RULE_FROM));
        const std::int64_t to = epoc::tz::symbian_time_to_unix_seconds(read_at<std::int64_t>(rules, rule + RULE_TO));
        REQUIRE(from < to);
        REQUIRE(to - from < 366 * 24 * 60 * 60);

        // Every time in the buffer is stated as UTC (ETzUtcTimeReference = 0).
        REQUIRE(read_at<std::int32_t>(rules, rule + RULE_FROM_REFERENCE) == 0);
        REQUIRE(read_at<std::int32_t>(rules, rule + RULE_TO_REFERENCE) == 0);
        REQUIRE(read_at<std::int32_t>(rules, rule + RULE_TIME_REFERENCE) == 0);

        // The two offsets are minutes, and London only ever moves by an hour.
        const std::int16_t old_offset = read_at<std::int16_t>(rules, rule + RULE_OLD_OFFSET);
        const std::int16_t new_offset = read_at<std::int16_t>(rules, rule + RULE_NEW_OFFSET);
        REQUIRE(((old_offset == 0) || (old_offset == 60)));
        REQUIRE(((new_offset == 0) || (new_offset == 60)));
        REQUIRE(old_offset != new_offset);

        // TMonth is zero-based (EJanuary = 0) and the day of month is too, which
        // is what TTzRule's constructor documents. Britain switches in March and
        // October.
        const std::int32_t month = read_at<std::int32_t>(rules, rule + RULE_MONTH);
        REQUIRE(((month == 2) || (month == 9)));
        REQUIRE(read_at<std::uint8_t>(rules, rule + RULE_DAY_OF_MONTH) <= 30);

        // ETzFixedDate = 0: the rule states a date, not "the last Sunday of".
        REQUIRE(read_at<std::int32_t>(rules, rule + RULE_DAY_RULE) == 0);

        // Minutes since midnight, so inside a day.
        REQUIRE(read_at<std::uint16_t>(rules, rule + RULE_TIME_OF_CHANGE) < 24 * 60);
    }
}

#endif

TEST_CASE("rules_clamp_the_years_a_client_asks_for", "timezone") {
    scoped_time_zone zone("UTC");

    // Symbian's own range is year 0 to 9999, and clients do ask for all of it.
    // Walking ten millennia of host transitions would block the server, so the
    // window is clamped -- but the header still answers the question asked.
    const std::vector<std::uint8_t> rules = epoc::tz::make_rules(-5, 12000);
    REQUIRE(read_at<std::int16_t>(rules, 0) == 0);
    REQUIRE(read_at<std::int16_t>(rules, 2) == 9998);

    // An end before the start is not a range; it collapses onto the start.
    const std::vector<std::uint8_t> backwards = epoc::tz::make_rules(2010, 1990);
    REQUIRE(read_at<std::int16_t>(backwards, 0) == 2010);
    REQUIRE(read_at<std::int16_t>(backwards, 2) == 2010);
}
