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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <catch2/catch.hpp>
#include <kernel/timer_deadline.h>
#include <kernel/timing.h>

#include <limits>

using namespace eka2l1::kernel;

// Symbian stimer.cpp fixes the nominal kernel tick at 15625 us. These fixtures
// describe that grid, not the hardware's additional nanokernel rounding jitter.
TEST_CASE("relative timers use the nominal global tick queue", "[timer]") {
    REQUIRE(timer_after_deadline(0, 0) == 15625);
    REQUIRE(timer_after_deadline(0, 1) == 15625);
    REQUIRE(timer_after_deadline(15624, 0) == 15625);
    REQUIRE(timer_after_deadline(15624, 1) == 15625);
    REQUIRE(timer_after_deadline(15625, 0) == 31250);
    REQUIRE(timer_after_deadline(15625, 1) == 31250);
    REQUIRE(timer_after_deadline(1000, 14625) == 15625);
    REQUIRE(timer_after_deadline(1000, 14626) == 31250);
    REQUIRE(timer_after_deadline(1000, 33000) == 46875);
}

// us_exec.cpp: RTimer::AfterTicks(n) calls Exec::TimerAfter(..., -n).
TEST_CASE("EKA2 TimerAfter decodes negative intervals as tick counts", "[timer]") {
    REQUIRE(timer_after_deadline(0, -1) == 15625);
    REQUIRE(timer_after_deadline(1000, -1) == 31250);
    REQUIRE(timer_after_deadline(1000, -64) == 1015625);
    REQUIRE(timer_after_deadline(1000000, -64) == 2000000);
    REQUIRE(timer_after_deadline(0, -2147483647) == 33554431984375ULL);
    REQUIRE(timer_after_deadline(0, std::numeric_limits<std::int32_t>::min()) == 33554431984375ULL);
    REQUIRE(timer_after_deadline(0, 2147483647) == 2147484375ULL);
    REQUIRE(timer_after_deadline(1000, -2147483647) == 33554431984375ULL);
    REQUIRE(tick_count_timer_deadline(1000, 64) == 1015625);
    REQUIRE(tick_count_timer_deadline(0, 0) == 15625);
    REQUIRE(tick_count_timer_deadline(15625, 0) == 31250);
}

// stimer.cpp rounds up the interval; nk_timer.cpp OneShot counts from the next tick.
TEST_CASE("high resolution timers wait on nanokernel tick boundaries", "[timer]") {
    REQUIRE(high_res_timer_deadline(0, 0) == 1000);
    REQUIRE(high_res_timer_deadline(999, 0) == 1000);
    REQUIRE(high_res_timer_deadline(1000, 0) == 2000);
    REQUIRE(high_res_timer_deadline(1001, 0) == 2000);
    REQUIRE(high_res_timer_deadline(250, 1) == 2000);
    REQUIRE(high_res_timer_deadline(250, 1000) == 2000);
    REQUIRE(high_res_timer_deadline(250, 1001) == 3000);
    REQUIRE(high_res_timer_deadline(999, 1000) == 2000);
    REQUIRE(high_res_timer_deadline(1000, 1000) == 3000);
    REQUIRE(high_res_timer_deadline(0, 2147483647) == 2147485000ULL);
}

TEST_CASE("absolute timer deadlines survive queue insertion", "[timer]") {
    eka2l1::ntimer timing(1000000);
    timing.reset();
    timing.stop();
    // Freeze the initialized clock and drive the event queue without a worker.
    const std::uint64_t now = timing.microseconds();
    std::uint64_t completed = 0;
    const int event = timing.register_event("absolute deadline test",
        [&completed](std::uint64_t data, int) { completed = data; });

    timing.schedule_event_at(now + 1000000, event, 42);
    REQUIRE(timing.advance() == std::optional<std::uint64_t>(1000000));
    REQUIRE(timing.unschedule_event(event, 42));
    timing.schedule_event_at(now, event, 42);
    REQUIRE_FALSE(timing.advance().has_value());
    REQUIRE(completed == 42);
}
