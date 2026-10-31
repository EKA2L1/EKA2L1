// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include "storage.h"
#include <atomic>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <control/service.h>
#include <ctime>
#include <future>
#include <iomanip>
#include <mutex>
#include <regex>
#include <sstream>
#include <thread>

namespace eka2l1::control {
    static bool query_bool(const request &req, const std::string &name, bool fallback) {
        if (!req.query.count(name))
            return fallback;
        const auto value = lower(req.param(name));
        return value.empty() || value == "1" || value == "true" || value == "yes";
    }
    using clock = std::chrono::steady_clock;
    static std::string timestamp() {
        auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm utc{};
#if defined(_WIN32)
        gmtime_s(&utc, &time);
#else
        gmtime_r(&time, &utc);
#endif
        std::ostringstream out;
        out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
        return out.str();
    }
    static const std::map<std::string, std::uint32_t> &key_names() {
        static const auto keys = [] {
            std::map<std::string, std::uint32_t> k = {
                { "lsk", 0xA4 }, { "softleft", 0xA4 }, { "menu", 0xA4 }, { "rsk", 0xA5 }, { "softright", 0xA5 }, { "back", 0xA5 },
                { "ok", 0xA7 }, { "select", 0xA7 }, { "center", 0xA7 }, { "up", 0x10 }, { "down", 0x11 }, { "left", 0x0E }, { "right", 0x0F },
                { "clear", 1 }, { "backspace", 1 }, { "call", 0xB4 }, { "green", 0xB4 }, { "end", 0xB5 }, { "red", 0xB5 }, { "edit", 0x12 }, { "shift", 0x12 },
                { "star", 0x2A }, { "*", 0x2A }, { "hash", 0x7F }, { "#", 0x7F }, { "space", 5 }, { "tab", 2 }, { "enter", 3 }, { "escape", 4 },
                { "yes", 0xC4 }, { "no", 0xC5 }, { "applications", 0x94 }
            };
            for (char c = '0'; c <= '9'; ++c)
                k[std::string(1, c)] = c;
            for (char c = 'a'; c <= 'z'; ++c)
                k[std::string(1, c)] = c - 32;
            return k;
        }();
        return keys;
    }
    static std::uint32_t scan_code(const json &v) {
        if (v.is_number()) {
            auto n = v.number();
            if (n >= 0 && n <= UINT32_MAX && n == std::floor(n))
                return static_cast<std::uint32_t>(n);
        } else if (v.is_string()) {
            auto name = lower(v.string());
            auto i = key_names().find(name);
            if (i != key_names().end())
                return i->second;
            if (name.substr(0, 2) == "0x")
                return parse_uid(name);
        }
        throw api_error(400, "bad_request", "unknown key; see GET /v1/keys");
    }
    struct service::impl : std::enable_shared_from_this<impl> {
        handler frontend;
        boost::asio::thread_pool workers{ 2 }, timed_workers{ 4 }, job_worker{ 1 };
        std::atomic<bool> stopped{ false };
        std::atomic<int> pending{ 0 }, timed_pending{ 0 };
        std::atomic<std::uint64_t> input_epoch{ 0 };
        std::mutex jobs_mutex, input_mutex, mutation_mutex;
        std::map<int, json> jobs;
        int next_job = 1;
        bool job_running = false;
        std::mutex sleep_mutex;
        std::condition_variable wake;

        explicit impl(handler frontend)
            : frontend(std::move(frontend)) {}
        void sleep(double ms, std::uint64_t epoch = UINT64_MAX) {
            if (!std::isfinite(ms) || ms < 0 || ms > 3600000)
                throw api_error(400, "bad_request", "duration must be between 0 and 3600000 ms");
            std::unique_lock<std::mutex> lock(sleep_mutex);
            wake.wait_for(lock, std::chrono::duration<double, std::milli>(ms), [&] { return stopped.load() || (epoch != UINT64_MAX && input_epoch.load() != epoch); });
            if (stopped)
                throw api_error(503, "stopped", "control service stopped");
            if (epoch != UINT64_MAX && input_epoch.load() != epoch)
                throw api_error(409, "cancelled", "input sequence cancelled by a session change");
        }
        response call(request req) {
            auto promise = std::make_shared<std::promise<response>>();
            auto future = promise->get_future();
            frontend(std::move(req), [promise](response res) { promise->set_value(std::move(res)); });
            while (future.wait_for(std::chrono::milliseconds(25)) != std::future_status::ready) {
                if (stopped)
                    throw api_error(503, "stopped", "control service stopped");
            }
            return future.get();
        }
        response checked(request req) {
            auto res = call(std::move(req));
            if (res.status >= 400) {
                auto e = json::parse(res.body)["error"];
                throw api_error(res.status, e["code"].string("internal"), e["message"].string("frontend failed"));
            }
            return res;
        }
        response storage(const request &req) {
            auto resolved = checked({ "RESOLVE", req.path, req.query, {}, {} });
            const auto path = required_string(json::parse(resolved.body), "hostPath");
            return req.path.back() == "files" ? host_files(req, path) : host_log(req, path);
        }
        response atomic_input(const std::string &kind, const json &data, const std::string &method = "POST") {
            return checked({ method, { "v1", "input", kind }, {}, {}, data.dump() });
        }
        void input_step(json step, std::string kind, std::uint64_t epoch) {
            auto send = [&](const std::string &kind, const json &data, const std::string &method = "POST") {
                if (input_epoch.load() != epoch)
                    throw api_error(409, "cancelled", "input sequence cancelled by a session change");
                return atomic_input(kind, data, method);
            };
            if (!step.is_object())
                throw api_error(400, "bad_request", "input step must be an object");
            if (kind.empty()) {
                if (step.contains("key") || step.contains("keys"))
                    kind = "key";
                else if (step.contains("touch"))
                    kind = "touch";
                else if (step.contains("text"))
                    kind = "text";
                else if (step.contains("accelerometer"))
                    kind = "accelerometer";
                else if (step.contains("waitMs"))
                    kind = "wait";
                else
                    throw api_error(400, "bad_request", "unrecognized input step");
            }
            if (kind == "key") {
                json::array scans;
                if (step.contains("keys")) {
                    if (!step["keys"].is_array() || step["keys"].items().empty())
                        throw api_error(400, "bad_request", "keys must be a nonempty array");
                    for (const auto &key : step["keys"].items())
                        scans.emplace_back(scan_code(key));
                } else
                    scans.emplace_back(scan_code(step["key"]));
                auto action = step["action"].string("tap");
                if (action == "down" || action == "up")
                    send("key", { { "keys", scans }, { "action", action } });
                else if (action == "tap") {
                    auto count = step["repeat"].number(1);
                    if (count < 1 || count > 10000 || count != std::floor(count))
                        throw api_error(400, "bad_request", "repeat must be 1..10000");
                    for (int i = 0; i < count; ++i) {
                        if (i)
                            sleep(step["gapMs"].number(80), epoch);
                        send("key", { { "keys", scans }, { "action", "down" } });
                        sleep(step["holdMs"].number(80), epoch);
                        auto release_scans = scans;
                        std::reverse(release_scans.begin(), release_scans.end());
                        send("key", { { "keys", release_scans }, { "action", "up" } });
                    }
                } else
                    throw api_error(400, "bad_request", "key action must be tap, down or up");
            } else if (kind == "text") {
                for (unsigned char c : lower(required_string(step, "text"))) {
                    auto key = c == ' ' ? "space" : c == '\n' ? "enter"
                                                              : std::string(1, c);
                    input_step({ { "key", key }, { "holdMs", 60 } }, "key", epoch);
                    sleep(step["intervalMs"].number(120), epoch);
                }
            } else if (kind == "touch") {
                auto touch = step.contains("touch") ? step["touch"] : step;
                auto action = touch["action"].string("tap");
                double x = required_number(touch, "x"), y = required_number(touch, "y");
                if (action == "down" || action == "move" || action == "up")
                    send("touch", touch);
                else if (action == "tap" || action == "swipe") {
                    double to_x = x, to_y = y;
                    const double duration = touch["durationMs"].number(300);
                    if (action == "swipe") {
                        to_x = required_number(touch, "toX");
                        to_y = required_number(touch, "toY");
                        if (!std::isfinite(duration) || duration < 0 || duration > 3600000)
                            throw api_error(400, "bad_request", "invalid swipe duration");
                    }
                    touch["action"] = "down";
                    send("touch", touch);
                    if (action == "tap")
                        sleep(touch["holdMs"].number(80), epoch);
                    else {
                        int count = std::max(2, static_cast<int>(duration / 16));
                        for (int i = 1; i <= count; ++i) {
                            sleep(duration / count, epoch);
                            touch["action"] = "move";
                            touch["x"] = x + (to_x - x) * i / count;
                            touch["y"] = y + (to_y - y) * i / count;
                            send("touch", touch);
                        }
                    }
                    touch["action"] = "up";
                    send("touch", touch);
                } else
                    throw api_error(400, "bad_request", "invalid touch action");
            } else if (kind == "accelerometer") {
                const auto &v = step["accelerometer"];
                send("accelerometer", v, v.is_null() ? "DELETE" : "PUT");
            }
            sleep(step["waitMs"].number(), epoch);
        }
        response wait(const request &req) {
            auto body = req.data();
            const auto until = required_string(body, "until");
            const auto timeout = body["timeoutMs"].number(30000);
            if (!std::isfinite(timeout) || timeout < 0 || timeout > 3600000)
                throw api_error(400, "bad_request", "invalid timeoutMs");
            const auto start = clock::now();
            auto status = [&] { return json::parse(checked({ "GET", { "v1", "status" }, {}, {}, {} }).body); };
            if (until == "sleep") {
                sleep(timeout);
                return response::from_json({ { "satisfied", true }, { "elapsedMs", std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count() }, { "status", status() } });
            }
            auto initial = status();
            const double target = initial["frames"].number() + std::max(1.0, body["count"].number(1));
            std::regex pattern;
            std::string offset;
            if (until == "log") {
                try {
                    pattern = std::regex(required_string(body, "pattern"));
                } catch (const std::regex_error &) {
                    throw api_error(400, "bad_request", "invalid pattern regex");
                }
                if (body.contains("since")) {
                    const auto since = required_number(body, "since");
                    if (since < 0 || since > 9007199254740991.0 || since != std::floor(since))
                        throw api_error(400, "bad_request", "invalid since offset");
                    offset = std::to_string(static_cast<std::uint64_t>(since));
                }
                if (offset.empty())
                    offset = storage({ "GET", { "v1", "log" }, { { "tail", "1" } }, {}, {} }).headers["X-Log-Offset"];
            } else if (until != "running" && until != "exited" && until != "idle" && until != "frames" && until != "sleep")
                throw api_error(400, "bad_request", "invalid wait condition");
            while (true) {
                auto current = status();
                auto state = current["session"]["state"].string();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
                bool done = until == "running" ? state == "running" : until == "exited" ? state != "running" && state != "launching"
                    : until == "frames"                                                 ? current["frames"].number() >= target
                    : until == "sleep"                                                  ? elapsed >= timeout
                                                                                        : false;
                if (until == "idle") {
                    std::lock_guard<std::mutex> guard(jobs_mutex);
                    done = !current["busy"].boolean(true) && !job_running;
                }
                if (until == "log") {
                    auto log = storage({ "GET", { "v1", "log" }, { { "since", offset } }, {}, {} });
                    const auto newline = log.body.rfind('\n');
                    if (newline != std::string::npos) {
                        auto complete = log.body.substr(0, newline + 1);
                        std::istringstream lines(complete);
                        std::string line;
                        while (std::getline(lines, line)) {
                            if (std::regex_search(line, pattern))
                                return response::from_json({ { "satisfied", true }, { "elapsedMs", elapsed }, { "line", line }, { "logOffset", static_cast<std::int64_t>(std::stoll(log.headers["X-Log-Offset"])) } });
                        }
                        offset = std::to_string(std::stoull(log.headers["X-Log-Offset"]) - (log.body.size() - newline - 1));
                    }
                }
                if (done)
                    return response::from_json({ { "satisfied", true }, { "elapsedMs", elapsed }, { "status", current } });
                if (elapsed >= timeout)
                    throw api_error(504, "timeout", until + " not reached within timeout");
                sleep(std::min(50.0, timeout - elapsed));
            }
        }
        static std::string job_kind(const request &req) {
            const auto &p = req.path;
            if (p.size() == 2 && p[1] == "devices" && req.method == "POST")
                return "install_device";
            if (p.size() == 3 && p[1] == "devices" && p[2] == "rescan" && req.method == "POST")
                return "rescan_devices";
            if (p.size() == 4 && p[1] == "devices" && p[3] == "boot" && req.method == "POST")
                return "boot_device";
            if (p.size() == 3 && p[1] == "devices" && req.method == "DELETE")
                return "delete_device";
            if (p.size() == 2 && p[1] == "apps" && req.method == "POST")
                return "install_app";
            if (p.size() == 2 && p[1] == "card" && req.method == "PUT")
                return "mount_card";
            return {};
        }
        response safe_route(request req) {
            try {
                return route(std::move(req));
            } catch (const api_error &e) {
                return response::error(e.status, e.code, e.what());
            } catch (const std::exception &e) {
                return response::error(500, "internal", e.what());
            }
        }
        response route(request req) {
            if (stopped)
                throw api_error(503, "stopped", "control service stopped");
            if (req.path.empty())
                req.path = { "v1" };
            if (req.path.front() != "v1")
                throw api_error(404, "not_found", "unknown API version");
            const auto &p = req.path;
            if (p.size() == 1 && req.method == "GET")
                return response::from_json({ { "api", 1 }, { "endpoints", json::array{ "GET /v1/status", "GET /v1/capabilities", "GET /v1/devices", "POST /v1/devices", "POST /v1/devices/rescan", "POST /v1/devices/{index|code}/boot", "PATCH /v1/devices/{index|code}", "DELETE /v1/devices/{index|code}", "GET /v1/apps?system=1", "POST /v1/apps", "DELETE /v1/apps/{uid}", "GET /v1/apps/{uid}/icon", "POST /v1/apps/{uid}/launch", "GET /v1/session", "DELETE /v1/session", "GET|PUT /v1/session/screen-mode", "GET|PUT /v1/session/frame-limit", "GET|PUT|DELETE /v1/card", "GET /v1/keys", "POST /v1/input/key", "POST /v1/input/touch", "POST /v1/input/text", "POST /v1/input/sequence", "POST /v1/input/release", "GET|PUT|DELETE /v1/input/accelerometer", "GET /v1/screenshot", "GET /v1/log", "POST /v1/wait", "GET|PATCH /v1/config", "GET|PUT /v1/language", "GET|PUT|DELETE /v1/files?path=", "POST /v1/emulator/pause", "POST /v1/emulator/resume", "GET /v1/jobs", "GET /v1/jobs/{id}" } } });
            if (p.size() >= 2 && p[1] == "jobs" && req.method == "GET") {
                std::lock_guard<std::mutex> guard(jobs_mutex);
                if (p.size() == 2) {
                    json::array list;
                    for (const auto &[id, job] : jobs)
                        list.push_back(job);
                    return response::from_json({ { "jobs", list } });
                }
                const auto id = parse_uid(p[2]);
                if (p.size() != 3 || !jobs.count(id))
                    throw api_error(404, "not_found", "no such job");
                return response::from_json(jobs.at(id));
            }
            const bool input = p.size() == 3 && p[1] == "input" && p[2] != "release";
            std::unique_lock<std::mutex> mutation;
            if (req.method != "GET" && !input && p != std::vector<std::string>{ "v1", "wait" }) {
                mutation = std::unique_lock<std::mutex>(mutation_mutex, std::try_to_lock);
                if (!mutation.owns_lock())
                    throw api_error(409, "busy", "another emulator operation is in flight");
            }
            auto kind = job_kind(req);
            if (!kind.empty()) {
                req.data();
                int id;
                {
                    std::lock_guard<std::mutex> guard(jobs_mutex);
                    if (job_running)
                        throw api_error(409, "busy", "another emulator operation is in flight");
                    job_running = true;
                    id = next_job++;
                    jobs[id] = { { "id", id }, { "kind", kind }, { "state", "running" }, { "progress", nullptr }, { "started", timestamp() } };
                    while (jobs.size() > 50)
                        jobs.erase(jobs.begin());
                }
                bool async = query_bool(req, "async", false);
                req.query.erase("async");
                req.progress = [weak = weak_from_this(), id](double fraction) {
                    if (auto self = weak.lock()) {
                        std::lock_guard<std::mutex> guard(self->jobs_mutex);
                        if (self->jobs.count(id))
                            self->jobs[id]["progress"] = std::clamp(fraction, 0.0, 1.0);
                    }
                };
                auto work = [self = shared_from_this(), req = std::move(req), id]() mutable {
                    const auto start = clock::now();
                    response res;
                    try {
                        ++self->input_epoch;
                        self->wake.notify_all();
                        std::lock_guard<std::mutex> input_guard(self->input_mutex);
                        res = self->call(std::move(req));
                    } catch (const api_error &e) {
                        res = response::error(e.status, e.code, e.what());
                    } catch (const std::exception &e) {
                        res = response::error(500, "internal", e.what());
                    }
                    json value;
                    try {
                        value = json::parse(res.body);
                    } catch (...) {
                        res = response::error(500, "internal", "frontend returned an invalid job result");
                        value = json::parse(res.body);
                    }
                    {
                        std::lock_guard<std::mutex> guard(self->jobs_mutex);
                        self->jobs[id]["state"] = res.status < 400 ? "succeeded" : "failed";
                        self->jobs[id][res.status < 400 ? "result" : "error"] = res.status < 400 ? value : value["error"];
                        self->jobs[id]["elapsedMs"] = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
                        self->jobs[id]["finished"] = timestamp();
                        self->job_running = false;
                    }
                    res.headers["X-Job-Id"] = std::to_string(id);
                    return res;
                };
                if (async) {
                    json job;
                    {
                        std::lock_guard<std::mutex> guard(jobs_mutex);
                        job = jobs[id];
                    }
                    boost::asio::post(job_worker, [work = std::move(work)]() mutable { work(); });
                    return response::from_json(job, 202);
                }
                return work();
            }
            if (p == std::vector<std::string>{ "v1", "wait" } && req.method == "POST")
                return wait(req);
            if (p == std::vector<std::string>{ "v1", "keys" } && req.method == "GET") {
                json names(json::object{});
                for (const auto &[name, scan] : key_names())
                    names[name] = hex(scan);
                return response::from_json({ { "keys", names } });
            }
            if (req.method != "GET" && p != std::vector<std::string>{ "v1", "wait" }) {
                std::lock_guard<std::mutex> guard(jobs_mutex);
                if (job_running)
                    throw api_error(409, "busy", "another emulator operation is in flight");
            }
            if (p.size() == 3 && p[1] == "input" && req.method == "POST"
                && (p[2] == "key" || p[2] == "touch" || p[2] == "text" || p[2] == "sequence")) {
                std::unique_lock<std::mutex> guard(input_mutex, std::try_to_lock);
                if (!guard.owns_lock())
                    throw api_error(409, "busy", "another input sequence is in flight");
                const auto data = req.data();
                const auto epoch = input_epoch.load();
                try {
                    if (p[2] == "sequence") {
                        if (!data["steps"].is_array())
                            throw api_error(400, "bad_request", "steps must be an array");
                        int index = 0;
                        for (const auto &step : data["steps"].items()) {
                            try {
                                input_step(step, {}, epoch);
                            } catch (const api_error &e) {
                                throw api_error(e.status, e.code, "step " + std::to_string(index) + ": " + e.what());
                            }
                            ++index;
                        }
                    } else
                        input_step(data, p[2], epoch);
                } catch (...) {
                    try {
                        atomic_input("release", json::object{});
                    } catch (...) {
                    }
                    throw;
                }
                return checked({ "GET", { "v1", "input", "accelerometer" }, {}, {}, {} });
            }
            if ((p == std::vector<std::string>{ "v1", "files" } && (req.method == "GET" || req.method == "PUT" || req.method == "DELETE"))
                || (p == std::vector<std::string>{ "v1", "log" } && req.method == "GET"))
                return storage(req);
            std::unique_lock<std::mutex> lifecycle_input;
            const bool changes_session = (p.size() == 4 && p[1] == "apps" && p[3] == "launch" && req.method == "POST")
                || (p == std::vector<std::string>{ "v1", "session" } && req.method == "DELETE")
                || (p == std::vector<std::string>{ "v1", "input", "release" } && req.method == "POST");
            if (changes_session) {
                ++input_epoch;
                wake.notify_all();
                lifecycle_input = std::unique_lock<std::mutex>(input_mutex);
            }
            const bool launch = p.size() == 4 && p[1] == "apps" && p[3] == "launch" && req.method == "POST";
            const bool wait_launch = launch && query_bool(req, "wait", true) && req.data()["wait"].boolean(true);
            const auto timeout = wait_launch ? req.data()["timeoutMs"].number(30000) : 0;
            if (!std::isfinite(timeout) || timeout < 0 || timeout > 3600000)
                throw api_error(400, "bad_request", "invalid timeoutMs");
            const bool get_status = p == std::vector<std::string>{ "v1", "status" } && req.method == "GET";
            const auto launch_epoch = input_epoch.load();
            auto res = call(std::move(req));
            if (lifecycle_input.owns_lock())
                lifecycle_input.unlock();
            if (mutation.owns_lock())
                mutation.unlock();
            if (wait_launch && res.status < 400) {
                auto state = json::parse(res.body);
                auto started = clock::now();
                while (state["state"].string() == "launching") {
                    if (input_epoch.load() != launch_epoch)
                        throw api_error(409, "cancelled", "app launch cancelled by a session change");
                    if (std::chrono::duration<double, std::milli>(clock::now() - started).count() >= timeout)
                        throw api_error(504, "timeout", "app launch did not complete");
                    sleep(50, launch_epoch);
                    state = json::parse(checked({ "GET", { "v1", "session" }, {}, {}, {} }).body);
                }
                if (state["state"].string() == "launch_failed")
                    throw api_error(422, "launch_failed", "app launch failed");
                return response::from_json(state);
            }
            if (get_status && res.status == 200) {
                auto data = json::parse(res.body);
                json::array running;
                {
                    std::lock_guard<std::mutex> guard(jobs_mutex);
                    for (const auto &[id, job] : jobs)
                        if (job["state"].string() == "running")
                            running.emplace_back(id);
                }
                data["runningJobs"] = running;
                res = response::from_json(data);
            }
            return res;
        }
    };
    service::service(handler frontend)
        : impl_(std::make_shared<impl>(std::move(frontend))) {}
    service::~service() { stop(); }
    void service::handle(request req, completion done) {
        auto self = impl_;
        if (!self || self->stopped) {
            done(response::error(503, "stopped", "control service stopped"));
            return;
        }
        const auto &p = req.path;
        const bool timed = (req.method == "POST" && p.size() == 2 && p[1] == "wait")
            || (req.method == "POST" && p.size() == 3 && p[1] == "input" && p[2] != "release")
            || (req.method == "POST" && p.size() == 4 && p[1] == "apps" && p[3] == "launch");
        auto &pending = timed ? self->timed_pending : self->pending;
        if (++pending > (timed ? 4 : 16)) {
            --pending;
            done(response::error(503, "busy", "too many pending operations"));
            return;
        }
        // Timed requests cannot consume the threads needed to close a session or poll jobs.
        boost::asio::post(timed ? self->timed_workers : self->workers, [self, timed, req = std::move(req), done = std::move(done)]() mutable {
            auto result = self->safe_route(std::move(req));
            --(timed ? self->timed_pending : self->pending);
            done(std::move(result));
        });
    }
    void service::stop() {
        if (!impl_)
            return;
        impl_->stopped = true;
        impl_->wake.notify_all();
        impl_->timed_workers.join();
        impl_->workers.join();
        impl_->job_worker.join();
        impl_.reset();
    }
}
