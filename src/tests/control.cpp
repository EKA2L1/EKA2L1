// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch.hpp>
#include <chrono>
#include <condition_variable>
#include <config/app_settings.h>
#include <config/config.h>
#include <control/service.h>
#include <control/native.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <thread>
#include <system/devices.h>
#include <system/epoc.h>

using namespace eka2l1::control;
using namespace std::chrono_literals;

static std::future<response> submit(service &control, request req) {
    auto promise = std::make_shared<std::promise<response>>();
    auto result = promise->get_future();
    control.handle(std::move(req), [promise](response result) { promise->set_value(std::move(result)); });
    return result;
}
static response call(service &control, request req) {
    auto result = submit(control, std::move(req));
    if (result.wait_for(2s) != std::future_status::ready) {
        control.stop();
        FAIL("control request did not complete");
    }
    return result.get();
}

TEST_CASE("Control input sequences preserve order and physical scan codes", "[control]") {
    std::vector<json> events;
    service control([&](request req, completion done) {
        if (req.method == "POST")
            events.push_back(req.data());
        done(response::from_json({ { "heldKeys", json::array{} }, { "heldPointers", json::array{} }, { "acceleration", nullptr } }));
    });
    auto res = call(control, { "POST", { "v1", "input", "sequence" }, {}, {}, R"({"steps":[{"keys":["shift","a"],"holdMs":0},{"touch":{"action":"swipe","x":1,"y":2,"toX":3,"toY":4,"durationMs":0}}]})" });
    REQUIRE(res.status == 200);
    REQUIRE(events.size() == 6);
    CHECK(events[0]["action"].string() == "down");
    CHECK(events[0]["keys"].items()[0].number() == 0x12);
    CHECK(events[0]["keys"].items()[1].number() == 0x41);
    CHECK(events[1]["action"].string() == "up");
    CHECK(events[2]["action"].string() == "down");
    CHECK(events[4]["x"].number() == 3);
    CHECK(events[4]["y"].number() == 4);
    CHECK(events[5]["action"].string() == "up");
}

TEST_CASE("Control rejects malformed input and releases held keys after a failing step", "[control]") {
    bool released = false;
    service control([&](request req, completion done) {
        if (req.path.back() == "release")
            released = true;
        done(response::from_json({}));
    });
    auto res = call(control, { "POST", { "v1", "input", "sequence" }, {}, {}, R"({"steps":[{"key":"5","action":"down"},{"key":"not-a-key"}]})" });
    REQUIRE(res.status == 400);
    CHECK(released);
    CHECK(json::parse(res.body)["error"]["message"].string().find("step 1") != std::string::npos);
    CHECK(call(control, { "POST", { "v1", "input", "key" }, {}, {}, "[]" }).status == 400);
    CHECK(call(control, { "POST", { "v1", "input", "key" }, {}, {}, R"({"key":-1})" }).status == 400);
}

TEST_CASE("Control jobs report progress, reject conflicting jobs, and preserve errors", "[control]") {
    std::promise<void> entered;
    request pending;
    completion finish;
    service control([&](request req, completion done) {
        pending = std::move(req);
        finish = std::move(done);
        entered.set_value();
    });
    auto ready = entered.get_future();
    auto accepted = call(control, { "PUT", { "v1", "card" }, { { "async", "1" } }, {}, R"({"path":"card.zip"})" });
    REQUIRE(accepted.status == 202);
    REQUIRE(ready.wait_for(2s) == std::future_status::ready);
    auto id = std::to_string(int(json::parse(accepted.body)["id"].number()));
    CHECK(call(control, { "POST", { "v1", "devices", "rescan" }, {}, {}, "{}" }).status == 409);
    pending.progress(0.5);
    auto job = json::parse(call(control, { "GET", { "v1", "jobs", id }, {}, {}, {} }).body);
    CHECK(job["state"].string() == "running");
    CHECK(job["progress"].number() == Approx(0.5));
    CHECK(job["started"].is_string());
    finish(response::error(422, "mount_failed", "corrupt archive"));
    for (int i = 0; i < 100; ++i) {
        job = json::parse(call(control, { "GET", { "v1", "jobs", id }, {}, {}, {} }).body);
        if (job["state"].string() != "running")
            break;
        std::this_thread::sleep_for(1ms);
    }
    REQUIRE(job["state"].string() == "failed");
    CHECK(job["error"]["code"].string() == "mount_failed");
    CHECK(job["finished"].is_string());
}

TEST_CASE("Closing a session cancels a long input hold before changing the session", "[control]") {
    std::promise<void> pressed;
    std::mutex mutex;
    std::vector<std::string> events;
    service control([&](request req, completion done) {
        {
            std::lock_guard<std::mutex> guard(mutex);
            events.push_back(req.path.back());
        }
        if (req.path.back() == "key")
            pressed.set_value();
        done(response::from_json({ { "state", "closed" } }));
    });
    auto ready = pressed.get_future();
    auto input = submit(control, { "POST", { "v1", "input", "key" }, {}, {}, R"({"key":"5","holdMs":60000})" });
    REQUIRE(ready.wait_for(2s) == std::future_status::ready);
    REQUIRE(call(control, { "DELETE", { "v1", "session" }, {}, {}, {} }).status == 200);
    REQUIRE(input.wait_for(2s) == std::future_status::ready);
    CHECK(input.get().status == 409);
    CHECK(events == std::vector<std::string>{ "key", "release", "session" });
}

TEST_CASE("Stopping control does not wait for a frontend completion or a sleep timeout", "[control]") {
    std::promise<void> entered;
    service control([&](request, completion) { entered.set_value(); });
    auto ready = entered.get_future();
    auto result = submit(control, { "GET", { "v1", "status" }, {}, {}, {} });
    REQUIRE(ready.wait_for(2s) == std::future_status::ready);
    control.stop();
    REQUIRE(result.wait_for(100ms) == std::future_status::ready);
    CHECK(result.get().status == 503);
}

TEST_CASE("Timed control requests leave capacity for session close and job polling", "[control]") {
    std::mutex mutex;
    std::condition_variable entered;
    int polls = 0;
    service control([&](request req, completion done) {
        if (req.path.back() == "status") {
            std::lock_guard<std::mutex> guard(mutex);
            ++polls;
            entered.notify_all();
        }
        done(response::from_json({ { "session", { { "state", "running" } } } }));
    });
    std::vector<std::future<response>> waits;
    for (int i = 0; i < 4; ++i)
        waits.push_back(submit(control, { "POST", { "v1", "wait" }, {}, {}, R"({"until":"exited","timeoutMs":60000})" }));
    {
        std::unique_lock<std::mutex> guard(mutex);
        REQUIRE(entered.wait_for(guard, 2s, [&] { return polls >= 8; }));
    }
    CHECK(call(control, { "GET", { "v1", "jobs" }, {}, {}, {} }).status == 200);
    CHECK(call(control, { "DELETE", { "v1", "session" }, {}, {}, {} }).status == 200);
    CHECK(call(control, { "POST", { "v1", "wait" }, {}, {}, R"({"until":"sleep","timeoutMs":60000})" }).status == 503);
    control.stop();
    for (auto &wait : waits)
        CHECK(wait.get().status == 503);
}

TEST_CASE("A launch waiting for the frontend can be closed", "[control]") {
    std::promise<void> launched;
    service control([&](request req, completion done) {
        if (req.path.back() == "launch")
            launched.set_value();
        done(response::from_json({ { "state", "launching" } }));
    });
    auto ready = launched.get_future();
    auto launch = submit(control, { "POST", { "v1", "apps", "0x10005902", "launch" }, {}, {}, R"({"timeoutMs":60000})" });
    REQUIRE(ready.wait_for(2s) == std::future_status::ready);
    CHECK(call(control, { "DELETE", { "v1", "session" }, {}, {}, {} }).status == 200);
    REQUIRE(launch.wait_for(2s) == std::future_status::ready);
    CHECK(launch.get().status == 409);
}

TEST_CASE("Common file and log operations use frontend paths and preserve binary content", "[control]") {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("eka-control-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    struct cleanup {
        fs::path path;
        ~cleanup() { fs::remove_all(path); }
    } remove{ root };
    const auto path = (root / "test.bin").string();
    bool saw_body = false;
    service control([&](request req, completion done) {
        saw_body |= !req.body.empty();
        done(response::from_json({ { "hostPath", path } }));
    });
    const std::map<std::string, std::string> query{ { "path", "docs:/test.bin" } };
    const std::string contents("a\0\xff\r\n", 5);
    REQUIRE(call(control, { "PUT", { "v1", "files" }, query, {}, contents }).status == 200);
    CHECK(call(control, { "GET", { "v1", "files" }, query, {}, {} }).body == contents);
    CHECK_FALSE(saw_body);
    {
        std::ofstream log(path, std::ios::binary);
        log << "before\nfull matching line\npartial";
    }
    auto found = call(control, { "POST", { "v1", "wait" }, {}, {}, R"({"until":"log","pattern":"matching","since":0,"timeoutMs":0})" });
    REQUIRE(found.status == 200);
    CHECK(json::parse(found.body)["line"].string() == "full matching line");
    CHECK(call(control, { "GET", { "v1", "log" }, { { "since", "-1" } }, {}, {} }).status == 400);
    CHECK(call(control, { "DELETE", { "v1", "files" }, query, {}, {} }).status == 200);
    CHECK_FALSE(fs::exists(path));
}

TEST_CASE("A native screenshot without an active surface fails before waiting on graphics", "[control]") {
    native_frontend frontend;
    bool touched_system = false;
    frontend.get_system = [&]() -> eka2l1::system * { touched_system = true; return nullptr; };
    frontend.can_capture = [] { return false; };
    native_backend backend(std::move(frontend));
    const auto result = backend.handle({ "GET", { "v1", "screenshot" }, {}, {}, {} });
    CHECK(result.status == 409);
    CHECK(json::parse(result.body)["error"]["code"].string() == "no_screen");
    CHECK_FALSE(touched_system);
}

TEST_CASE("Native control status is available with no installed devices", "[control][no-device]") {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("eka-control-no-device-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    struct cleanup {
        fs::path path;
        ~cleanup() { fs::remove_all(path); }
    } remove{ root };
    eka2l1::config::state conf;
    conf.storage = root.string() + "/";
    eka2l1::config::app_settings settings(&conf);
    eka2l1::system_create_components components;
    components.conf_ = &conf;
    components.settings_ = &settings;
    components.cache_root_ = conf.storage + "cache/";
    eka2l1::system sys(components);
    REQUIRE(sys.get_device_manager()->total() == 0);
    SECTION("Before core startup") {
        REQUIRE(sys.get_kernel_system() == nullptr);
    }
    SECTION("After core startup without a window server") {
        sys.startup();
        REQUIRE(sys.get_kernel_system() != nullptr);
    }

    native_frontend frontend;
    frontend.get_system = [&] { return &sys; };
    frontend.frames = [] { return 0; };
    native_backend backend(std::move(frontend));
    service control([&](request req, completion done) { done(backend.handle(req)); });
    for (int i = 0; i < 2; ++i) {
        const auto result = call(control, { "GET", { "v1", "status" }, {}, {}, {} });
        REQUIRE(result.status == 200);
        const auto status = json::parse(result.body);
        CHECK(status["started"].boolean());
        CHECK(status["device"].is_null());
        CHECK(status["screen"].is_null());
        CHECK(status["session"]["state"].string() == "none");
    }
    const auto devices = call(control, { "GET", { "v1", "devices" }, {}, {}, {} });
    REQUIRE(devices.status == 200);
    CHECK(json::parse(devices.body)["devices"].items().empty());
}
