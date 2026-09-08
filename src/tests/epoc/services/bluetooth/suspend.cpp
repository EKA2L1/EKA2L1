#include <catch2/catch.hpp>
#include <services/bluetooth/protocols/btmidman_inet.h>
#include <services/bluetooth/protocols/common_inet.h>

#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace {
    using namespace eka2l1::epoc::bt;
    using namespace std::chrono_literals;

    template <typename F>
    auto on_loop(F fn) {
        using result_type = std::invoke_result_t<F>;
        auto task = std::make_shared<std::packaged_task<result_type()>>(std::move(fn));
        auto result = task->get_future();
        libuv::default_looper->one_shot([task]() { (*task)(); });
        if (result.wait_for(5s) != std::future_status::ready) {
            throw std::runtime_error("Bluetooth loop did not respond");
        }
        return result.get();
    }

    template <typename F>
    bool eventually(F fn) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        do {
            if (on_loop(fn)) return true;
            std::this_thread::sleep_for(10ms);
        } while (std::chrono::steady_clock::now() < deadline);
        return false;
    }

    std::vector<std::shared_ptr<uvw::udp_handle>> udp_sockets() {
        std::vector<std::shared_ptr<uvw::udp_handle>> sockets;
        uv_walk(uv_default_loop(), [](uv_handle_t *handle, void *data) {
            if (handle->type == UV_UDP && !uv_is_closing(handle)) {
                auto &sockets = *static_cast<std::vector<std::shared_ptr<uvw::udp_handle>>*>(data);
                sockets.push_back(static_cast<uvw::udp_handle*>(handle->data)->shared_from_this());
            }
        }, &sockets);
        return sockets;
    }

    struct configuration : eka2l1::config::state {
        explicit configuration(discovery_mode mode) {
            btnet_discovery_mode = mode;
            internet_bluetooth_port = 0;
            enable_upnp = false;
            bt_central_server_url = "::1";
        }
    };

    void receive_error(uvw::udp_handle &socket, int error) {
        const uv_buf_t buffer = uv_buf_init(nullptr, 0);
        socket.raw()->recv_cb(socket.raw(), error, &buffer, nullptr, 0);
    }

    sockaddr_in6 loopback(unsigned int port) {
        sockaddr_in6 address{};
        uv_ip6_addr("::1", port, &address);
        return address;
    }

    std::vector<char> query_address(unsigned int port) {
        auto response = std::make_shared<std::promise<std::vector<char>>>();
        auto result = response->get_future();
        auto client = on_loop([=]() {
            auto client = uvw::loop::get_default()->resource<uvw::udp_handle>();
            const auto address = loopback(0);
            client->bind(reinterpret_cast<const sockaddr&>(address));
            client->on<uvw::udp_data_event>([response](const auto &event, auto &handle) {
                handle.stop();
                response->set_value({event.data.get(), event.data.get() + event.length});
            });
            client->recv();
            // Four-byte opaque asker ID, then the virtual-address query opcode.
            const char request[] = {1, 0, 0, 0, 1};
            const auto destination = loopback(port);
            client->send(reinterpret_cast<const sockaddr&>(destination),
                copy_control_packet(request, sizeof(request)), sizeof(request));
            return client;
        });
        const bool ready = result.wait_for(2s) == std::future_status::ready;
        on_loop([&]() { shutdown_uv_handle(client); });
        return ready ? result.get() : std::vector<char>{};
    }

    struct proxy_server {
        std::shared_ptr<uvw::tcp_handle> listener;
        std::vector<std::shared_ptr<uvw::tcp_handle>> peers;
        unsigned int logins = 0;

        proxy_server() {
            if (!libuv::default_looper->started()) libuv::default_looper->start();
            on_loop([this]() {
                listener = uvw::loop::get_default()->resource<uvw::tcp_handle>();
                const auto address = loopback(CENTRAL_SERVER_STANDARD_PORT);
                if (listener->bind(reinterpret_cast<const sockaddr&>(address)) < 0 || listener->listen() < 0) {
                    shutdown_uv_handle(listener);
                    throw std::runtime_error("Test proxy port is unavailable");
                }
                listener->on<uvw::listen_event>([this](const auto &, auto &server) {
                    auto peer = server.parent().template resource<uvw::tcp_handle>();
                    server.accept(*peer);
                    peer->template on<uvw::data_event>([this](const auto &event, auto &) {
                        if (event.length >= 2 && event.data[0] == 9) ++logins;
                    });
                    peer->read();
                    peers.push_back(peer);
                });
            });
        }

        ~proxy_server() {
            on_loop([this]() {
                for (auto &peer : peers) shutdown_uv_handle(peer);
                shutdown_uv_handle(listener);
            });
        }

        // Inject at uvw's receive boundary so the half-packet is processed before suspension.
        void deliver(std::vector<char> bytes) {
            on_loop([&]() {
                uv_walk(uv_default_loop(), [](uv_handle_t *handle, void *data) {
                    if (handle->type != UV_TCP || uv_is_closing(handle)) return;
                    sockaddr_in6 peer{};
                    int size = sizeof(peer);
                    if (uv_tcp_getpeername(reinterpret_cast<uv_tcp_t*>(handle),
                            reinterpret_cast<sockaddr*>(&peer), &size) != 0 ||
                        ntohs(peer.sin6_port) != CENTRAL_SERVER_STANDARD_PORT) return;
                    auto &bytes = *static_cast<std::vector<char>*>(data);
                    auto *stream = reinterpret_cast<uv_stream_t*>(handle);
                    const uv_buf_t buffer = uv_buf_init(copy_control_packet(bytes.data(), bytes.size()).release(),
                        static_cast<unsigned int>(bytes.size()));
                    stream->read_cb(stream, bytes.size(), &buffer);
                }, &bytes);
            });
        }
    };

    struct observer : inet_stranger_call_observer {
        std::vector<eka2l1::epoc::socket::saddress> found;
        unsigned int completions = 0;
        void on_stranger_call(eka2l1::epoc::socket::saddress &address, std::uint32_t) override {
            found.push_back(address);
        }
        void on_no_more_strangers() override { ++completions; }
    };
}

TEST_CASE("Bluetooth discovery survives transient UDP errors and rebuilds dead sockets", "[.btinet]") {
    midman_inet midman{configuration{DISCOVERY_MODE_DIRECT_IP}};
    auto sockets = on_loop(udp_sockets);
    REQUIRE(sockets.size() == 1);
    auto socket = sockets.front();
    for (const int error : {UV_ENETDOWN, UV_EINVAL}) {
        on_loop([&]() { receive_error(*socket, error); });
        REQUIRE(on_loop([&]() { return socket->active(); }));
        const auto response = query_address(on_loop([&]() { return socket->sock().port; }));
        REQUIRE(response.size() == 5 + sizeof(device_address));
        CHECK(response[4] == 101);
    }

    on_loop([&]() { receive_error(*socket, UV_ENOTCONN); });
    CHECK_FALSE(on_loop([&]() { return socket->active(); }));
    for (int cycle = 0; cycle < 10; ++cycle) {
        midman.suspend();
        midman.suspend();
        REQUIRE(on_loop(udp_sockets).empty());
        midman.resume();
        midman.resume();
        REQUIRE(on_loop(udp_sockets).size() == 1);
    }
    sockets = on_loop(udp_sockets);
    const auto response = query_address(on_loop([&]() { return sockets.front()->sock().port; }));
    REQUIRE(response.size() == 5 + sizeof(device_address));
}

TEST_CASE("Bluetooth proxy reconnect discards a partial reply from the old connection", "[.btinet]") {
    proxy_server proxy;
    observer search;
    midman_inet midman{configuration{DISCOVERY_MODE_PROXY_SERVER}};
    REQUIRE(eventually([&]() { return proxy.logins == 1; }));
    // Player-list reply: one IPv4 endpoint with explicit port, interrupted mid-address.
    proxy.deliver({5, 1, 2, 127});
    midman.suspend();
    midman.resume();
    REQUIRE(eventually([&]() { return proxy.logins == 2; }));
    midman.begin_hearing_stranger_call(&search);
    proxy.deliver({5, 1, 2, 127, 0, 0, 1, 0x12, 0x34});
    const auto found = on_loop([&]() { return search.found; });
    REQUIRE(found.size() == 1);
    CHECK(found[0].port_ == 0x1234);
    const auto &address = reinterpret_cast<const eka2l1::epoc::internet::sinet6_address&>(found[0]);
    CHECK(address.get_address_32x4()[3] == htonl(0x7F000001));
    CHECK(on_loop([&]() { return search.completions; }) == 1);
    midman.unregister_stranger_call_observer(&search);
}
