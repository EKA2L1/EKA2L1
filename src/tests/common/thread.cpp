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

#include <common/platform.h>
#include <common/thread.h>

#if EKA2L1_PLATFORM(DARWIN)
#include <pthread.h>
#include <pthread/qos.h>

#include <thread>

using namespace eka2l1;

namespace {
    qos_class_t qos_after_setting(const common::thread_priority pri) {
        qos_class_t observed = QOS_CLASS_UNSPECIFIED;

        // On its own thread: the request is permanent for the calling thread,
        // and the rest of the suite should not inherit it.
        std::thread worker([&observed, pri]() {
            common::set_thread_priority(pri);

            int relative = 0;
            pthread_get_qos_class_np(pthread_self(), &observed, &relative);
        });

        worker.join();
        return observed;
    }
}

// <pthread/qos.h>: pthread_setschedparam() "will unset the QOS class ... A thread
// so modified is permanently opted-out of the QOS class system and calls to this
// function to request a QOS class for such a thread will fail and return EPERM."
//
// So the observable contract is not just "some priority was applied": a thread
// that went through set_thread_priority must report an actual QoS class rather
// than QOS_CLASS_UNSPECIFIED, which is what the portable sched_param path leaves
// behind.

TEST_CASE("darwin_thread_priority_requests_a_qos_class", "common_thread") {
    REQUIRE(qos_after_setting(common::thread_priority_low) != QOS_CLASS_UNSPECIFIED);
    REQUIRE(qos_after_setting(common::thread_priority_normal) != QOS_CLASS_UNSPECIFIED);
    REQUIRE(qos_after_setting(common::thread_priority_high) != QOS_CLASS_UNSPECIFIED);
    REQUIRE(qos_after_setting(common::thread_priority_very_high) != QOS_CLASS_UNSPECIFIED);
}

TEST_CASE("darwin_thread_priority_order_survives_the_qos_mapping", "common_thread") {
    // QoS classes are ordered by their enumerator values (qos.h lists them from
    // BACKGROUND up to USER_INTERACTIVE), so a higher priority must not map to a
    // class the scheduler treats as lower.
    const qos_class_t low = qos_after_setting(common::thread_priority_low);
    const qos_class_t normal = qos_after_setting(common::thread_priority_normal);
    const qos_class_t high = qos_after_setting(common::thread_priority_high);
    const qos_class_t very_high = qos_after_setting(common::thread_priority_very_high);

    REQUIRE(low < normal);
    REQUIRE(normal < high);
    REQUIRE(high < very_high);

    // Nothing should be parked on the efficiency-only class.
    REQUIRE(low > QOS_CLASS_BACKGROUND);
}
#endif
