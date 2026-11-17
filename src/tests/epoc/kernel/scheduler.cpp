// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch.hpp>

#include <config/app_settings.h>
#include <config/config.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/thread.h>
#include <kernel/timing.h>
#include <mem/mem.h>
#include <system/epoc.h>

using namespace eka2l1;

TEST_CASE("timeslices preserve strict priority and rotate equal-priority threads", "[scheduler]") {
    config::state config;
    config.cpu_backend = "dyncom";
    config.cpu_load_save = false;
    config::app_settings settings(&config);
    std::unique_ptr<memory_system> memory;
    system_create_components components;
    components.conf_ = &config;
    components.settings_ = &settings;
    eka2l1::system system(components);
    system.startup();
    auto *kernel = system.get_kernel_system();
    auto *timing = kernel->get_ntimer();
    timing->stop();
    memory = std::make_unique<memory_system>(nullptr, &config, mem::mem_model_type::multiple, false);
    kernel->install_memory(memory.get());
    auto *process = kernel->create<kernel::process>(memory.get(), "scheduler test", guest_path{}, u"");
    auto make_thread = [&](const char *name, kernel::thread_priority priority) {
        return kernel->create<kernel::thread>(memory.get(), timing, process, kernel::access_type::local_access,
            name, 0, 0x1000, 0, 0, true, ptr<void>(0), ptr<void>(0), priority);
    };
    auto *low = make_thread("low", kernel::priority_absolute_low);
    auto *high = make_thread("high", kernel::priority_real_time);
    auto *scheduler = kernel->get_thread_scheduler();
    low->resume();
    high->resume();
    scheduler->reschedule();
    REQUIRE(scheduler->current_thread() == high);

    SECTION("an expired high-priority thread keeps running ahead of lower priorities") {
        for (int i = 0; i < 3; ++i) {
            high->add_ticks(high->get_remaining_screenticks());
            scheduler->reschedule();
            REQUIRE(scheduler->current_thread() == high);
            REQUIRE(high->get_remaining_screenticks() == 0);
            REQUIRE(high->get_execution_budget() > 0);
        }
        high->suspend();
        scheduler->reschedule();
        REQUIRE(scheduler->current_thread() == low);
    }

    SECTION("an expired lone thread yields as soon as an equal-priority peer becomes ready") {
        high->add_ticks(high->get_remaining_screenticks());
        scheduler->reschedule();
        REQUIRE(scheduler->current_thread() == high);
        REQUIRE(high->get_remaining_screenticks() == 0);
        auto *peer = make_thread("peer", kernel::priority_real_time);
        peer->resume();
        scheduler->reschedule();
        REQUIRE(scheduler->current_thread() == peer);
        REQUIRE(high->get_remaining_screenticks() > 0);
    }

    SECTION("an expired thread rotates among its equal-priority peers") {
        auto *peer = make_thread("peer", kernel::priority_real_time);
        peer->resume();
        high->add_ticks(high->get_remaining_screenticks());
        scheduler->reschedule();
        REQUIRE(scheduler->current_thread() == peer);
        peer->add_ticks(peer->get_remaining_screenticks());
        scheduler->reschedule();
        REQUIRE(scheduler->current_thread() == high);
    }
}
