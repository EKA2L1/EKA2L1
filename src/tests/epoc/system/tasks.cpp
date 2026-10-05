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

#include <config/app_settings.h>
#include <config/config.h>
#include <system/epoc.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace eka2l1;

namespace {
    struct task_fixture {
        config::state conf;
        config::app_settings settings;
        system_create_components components;
        std::unique_ptr<eka2l1::system> sys;

        explicit task_fixture()
            : settings(&conf) {
            components.conf_ = &conf;
            components.settings_ = &settings;

            sys = std::make_unique<eka2l1::system>(components);
            sys->startup();
        }
    };
}

TEST_CASE("pending_tasks_run_in_posting_order_on_the_caller_when_no_loop_runs", "[system_tasks]") {
    task_fixture fixture;
    std::vector<int> order;

    fixture.sys->post_task([&order]() { order.push_back(1); });
    fixture.sys->post_task([&order]() { order.push_back(2); });

    REQUIRE(order.empty());
    REQUIRE(fixture.sys->try_run_pending_tasks());
    REQUIRE(order == std::vector<int>{ 1, 2 });

    // Each task runs once.
    REQUIRE(fixture.sys->try_run_pending_tasks());
    REQUIRE(order == std::vector<int>{ 1, 2 });
}

TEST_CASE("a_task_may_post_another_task", "[system_tasks]") {
    task_fixture fixture;
    std::vector<int> order;

    fixture.sys->post_task([&]() {
        order.push_back(1);
        fixture.sys->post_task([&order]() { order.push_back(2); });
    });

    REQUIRE(fixture.sys->try_run_pending_tasks());
    REQUIRE(order == std::vector<int>{ 1 });

    REQUIRE(fixture.sys->try_run_pending_tasks());
    REQUIRE(order == std::vector<int>{ 1, 2 });
}

TEST_CASE("tasks_posted_from_many_threads_all_run", "[system_tasks]") {
    task_fixture fixture;
    std::atomic<int> ran{ 0 };

    std::vector<std::thread> posters;
    for (int i = 0; i < 4; i++) {
        posters.emplace_back([&]() {
            for (int j = 0; j < 100; j++) {
                fixture.sys->post_task([&ran]() { ran++; });
            }
        });
    }

    for (auto &poster : posters) {
        poster.join();
    }

    REQUIRE(fixture.sys->try_run_pending_tasks());
    REQUIRE(ran == 400);
}

TEST_CASE("tasks_do_not_run_while_another_thread_owns_the_system", "[system_tasks]") {
    task_fixture fixture;
    std::atomic<bool> started{ false };
    std::atomic<bool> release{ false };

    fixture.sys->post_task([&]() {
        started = true;
        while (!release) {
            std::this_thread::yield();
        }
    });

    std::thread owner([&]() { fixture.sys->try_run_pending_tasks(); });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!started && (std::chrono::steady_clock::now() < deadline)) {
        std::this_thread::yield();
    }

    if (!started) {
        release = true;
        owner.join();
    }

    REQUIRE(started);

    bool second_ran = false;
    fixture.sys->post_task([&second_ran]() { second_ran = true; });

    // The system is held by the thread running the first task.
    REQUIRE_FALSE(fixture.sys->try_run_pending_tasks());
    REQUIRE_FALSE(second_ran);

    release = true;
    owner.join();

    REQUIRE(fixture.sys->try_run_pending_tasks());
    REQUIRE(second_ran);
}
