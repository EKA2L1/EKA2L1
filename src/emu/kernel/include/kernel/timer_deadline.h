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

#pragma once

#include <common/common.h>

#include <limits>

namespace eka2l1::kernel {
    // Model the nominal 64 Hz queue; hardware also has nanokernel rounding jitter.
    constexpr std::uint64_t tick_timer_deadline(std::uint64_t now, std::uint64_t interval_us) {
        constexpr std::uint64_t period = 1000000 / epoc::TICK_TIMER_HZ;
        const std::uint64_t deadline = now + (interval_us ? interval_us : 1);
        return (deadline + period - 1) / period * period;
    }

    constexpr std::uint64_t tick_count_timer_deadline(std::uint64_t now, std::uint32_t tick_count) {
        if (tick_count == 0) {
            return tick_timer_deadline(now, 0);
        }

        constexpr std::uint64_t period = 1000000 / epoc::TICK_TIMER_HZ;
        constexpr std::uint64_t max_ticks = std::numeric_limits<std::int32_t>::max();
        const std::uint64_t adjusted = static_cast<std::uint64_t>(tick_count) + (now % period != 0);
        // MicroSecondsToTicks saturates the adjusted tick count at KMaxTInt.
        return now / period * period + (adjusted > max_ticks ? max_ticks : adjusted) * period;
    }

    // EKA2 RTimer::AfterTicks passes -aTicks through Exec::TimerAfter.
    constexpr std::uint64_t timer_after_deadline(std::uint64_t now, std::int32_t interval) {
        if (interval < 0) {
            return tick_count_timer_deadline(now, static_cast<std::uint32_t>(-static_cast<std::int64_t>(interval)));
        }
        return tick_timer_deadline(now, static_cast<std::uint64_t>(interval));
    }

    constexpr std::uint64_t high_res_timer_deadline(std::uint64_t now, std::uint32_t interval_us) {
        constexpr std::uint64_t period = 1000000 / epoc::NANOKERNEL_HZ;
        const std::uint64_t ticks = (static_cast<std::uint64_t>(interval_us) + period - 1) / period;
        // NTimer::OneShot counts from the next nanokernel tick, including for zero.
        return (now / period + 1 + ticks) * period;
    }
}
