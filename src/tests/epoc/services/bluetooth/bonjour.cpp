#ifdef __APPLE__
#include <catch2/catch.hpp>
#include <services/bluetooth/protocols/bonjour.h>
#include <uv.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

namespace {
    template <typename Predicate>
    bool await_bonjour(Predicate predicate) {
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(12);
        do {
            uv_run(uv_default_loop(), UV_RUN_NOWAIT);
            if (predicate()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while (std::chrono::steady_clock::now() < end);
        return false;
    }
}

TEST_CASE("Bonjour discovers matching rooms and withdraws departed peers", "[.bonjour]") {
    using namespace eka2l1::epoc::bt;
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string room = "eka2l1-test-" + std::to_string(nonce);
    device_address first{};
    std::memcpy(first.addr_, &nonce, 6);
    device_address second = first;
    device_address third = first;
    second.addr_[0] ^= 0x40;
    third.addr_[0] ^= 0x80;
    auto observer = std::make_unique<bonjour_discovery>(first, room, 35681, [] {});
    auto peer = std::make_unique<bonjour_discovery>(second, room, 35682, [] {});
    auto other_room = std::make_unique<bonjour_discovery>(third, room + "-other", 35683, [] {});

    REQUIRE(await_bonjour([&] { return !observer->peers().empty() && !peer->peers().empty(); }));
    for (const auto &found : observer->peers()) {
        CHECK(std::memcmp(found.address.addr_, second.addr_, 6) == 0);
        CHECK(found.endpoint.port_ == 35682);
    }
    CHECK(other_room->peers().empty());

    peer.reset();
    REQUIRE(await_bonjour([&] { return observer->peers().empty(); }));
    for (int i = 0; i < 10; ++i) {
        auto cancelled = std::make_unique<bonjour_discovery>(second, room, 35682, [] {});
        cancelled.reset();
        uv_run(uv_default_loop(), UV_RUN_NOWAIT);
    }
    other_room.reset();
    observer.reset();
    uv_run(uv_default_loop(), UV_RUN_NOWAIT);
}
#endif
