// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch.hpp>

#include <config/app_settings.h>
#include <config/config.h>
#include <kernel/kernel.h>
#include <services/internet/protocols/inet.h>
#include <services/socket/server.h>
#include <system/epoc.h>
#include <utils/err.h>

#include <chrono>
#include <future>

using namespace eka2l1;

TEST_CASE("TCP connect survives close before and after native submission", "[.inetconnect]") {
    config::state config;
    config::app_settings settings(&config);
    system_create_components components;
    components.conf_ = &config;
    components.settings_ = &settings;
    eka2l1::system system(components);
    system.startup();
    auto *kernel = system.get_kernel_system();
    epoc::internet::inet_bridged_protocol protocol(kernel, true, epoc::internet::INET_TCP_PROTOCOL_ID);
    auto loop = protocol.get_looper();

    kernel->lock();
    auto socket = protocol.make_socket(epoc::internet::INET_ADDRESS_FAMILY,
        epoc::internet::INET_TCP_PROTOCOL_ID, epoc::socket::socket_type_stream);
    kernel->unlock();
    REQUIRE(socket);

    uv_tcp_t server{};
    std::promise<int> listening;
    loop->one_shot([&] {
        uv_tcp_init(loop->raw_loop(), &server);
        sockaddr_in address{};
        uv_ip4_addr("127.0.0.1", 0, &address);
        const int bind_error = uv_tcp_bind(&server, reinterpret_cast<sockaddr *>(&address), 0);
        const int listen_error = uv_listen(reinterpret_cast<uv_stream_t *>(&server), 16, [](uv_stream_t *, int) {});
        int size = sizeof(address);
        uv_tcp_getsockname(&server, reinterpret_cast<sockaddr *>(&address), &size);
        listening.set_value(bind_error ? bind_error : (listen_error ? listen_error : ntohs(address.sin_port)));
    });
    auto ready = listening.get_future();
    REQUIRE(ready.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    const int port = ready.get();
    REQUIRE(port > 0);

    epoc::internet::sinet_address destination{};
    destination.family_ = epoc::internet::INET_ADDRESS_FAMILY;
    destination.port_ = port;
    *destination.addr_long() = 0x7F000001;
    epoc::notify_info notification;
    std::promise<void> closed;
    uv_idle_t check{};
    void *native_handle = static_cast<epoc::internet::inet_socket *>(socket.get())->get_opaque_handle();
    bool observed_submission = false;

    const bool submitted = GENERATE(false, true);
    const bool cancel = GENERATE(false, true);
    kernel->lock();
    socket->connect(destination, notification);
    if (cancel && !submitted) {
        socket->cancel_connect();
    }
    if (!submitted) {
        socket.reset();
    }
    loop->one_shot([&] {
        uv_idle_init(loop->raw_loop(), &check);
        struct close_check {
            kernel_system *kernel;
            std::unique_ptr<epoc::socket::socket> *socket;
            void *handle;
            bool *observed_submission;
            bool cancel;
            std::chrono::steady_clock::time_point deadline;
            std::promise<void> *closed;
        };
        check.data = new close_check{kernel, &socket, native_handle, &observed_submission, cancel,
            std::chrono::steady_clock::now() + std::chrono::seconds(2), &closed};
        uv_idle_start(&check, [](uv_idle_t *check) {
            auto *state = static_cast<close_check *>(check->data);
            if (*state->socket) {
                sockaddr_in6 peer{};
                int size = sizeof(peer);
                *state->observed_submission = check->loop->active_reqs.count != 0
                    || uv_tcp_getpeername(static_cast<uv_tcp_t *>(state->handle),
                        reinterpret_cast<sockaddr *>(&peer), &size) == 0;
                if (!*state->observed_submission && std::chrono::steady_clock::now() < state->deadline) {
                    return;
                }
                state->kernel->lock();
                if (state->cancel) {
                    (*state->socket)->cancel_connect();
                }
                state->socket->reset();
                state->kernel->unlock();
            }
            struct handle_search { void *handle; bool found = false; } result{state->handle};
            uv_walk(check->loop, [](uv_handle_t *handle, void *data) {
                auto *search = static_cast<handle_search *>(data);
                search->found |= handle == search->handle;
            }, &result);
            if (!result.found) {
                uv_idle_stop(check);
                uv_close(reinterpret_cast<uv_handle_t *>(check), [](uv_handle_t *handle) {
                    auto *state = static_cast<close_check *>(handle->data);
                    state->closed->set_value();
                    delete state;
                });
            }
        });
    });
    kernel->unlock();

    auto done = closed.get_future();
    REQUIRE(done.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    REQUIRE(observed_submission == submitted);

    std::promise<void> server_closed;
    server.data = &server_closed;
    loop->one_shot([&] {
        uv_close(reinterpret_cast<uv_handle_t *>(&server), [](uv_handle_t *handle) {
            static_cast<std::promise<void> *>(handle->data)->set_value();
        });
    });
    REQUIRE(server_closed.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready);
}

TEST_CASE("Nonblocking TCP sockets report readiness and cancel select on close", "[.inetconnect]") {
    config::state config;
    config::app_settings settings(&config);
    system_create_components components;
    components.conf_ = &config;
    components.settings_ = &settings;
    eka2l1::system system(components);
    system.startup();
    auto *kernel = system.get_kernel_system();
    epoc::internet::inet_bridged_protocol protocol(kernel, false, epoc::internet::INET_TCP_PROTOCOL_ID);
    auto loop = protocol.get_looper();
    kernel->lock();
    auto socket = protocol.make_socket(epoc::internet::INET_ADDRESS_FAMILY,
        epoc::internet::INET_TCP_PROTOCOL_ID, epoc::socket::socket_type_stream);
    kernel->unlock();
    REQUIRE(socket);

    auto option = [&](std::uint32_t id) {
        const std::lock_guard<kernel_system> guard(*kernel);
        std::uint32_t value = 0;
        REQUIRE(socket->get_option(id, epoc::socket::SOCKET_OPTION_FAMILY_BASE,
            reinterpret_cast<std::uint8_t *>(&value), sizeof(value)) == sizeof(value));
        return value;
    };
    auto await = [](auto predicate) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!predicate() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return predicate();
    };
    REQUIRE(option(epoc::socket::SOCKET_OPTION_ID_BLOCKING_IO) == 1);
    protocol_description description{};
    REQUIRE(socket->info(description));
    REQUIRE(description.addr_fam_ == 0x0800);
    REQUIRE(description.sock_type_ == 1);
    REQUIRE(description.protocol_ == 6);
    REQUIRE(socket->set_option(epoc::socket::SOCKET_OPTION_ID_NON_BLOCKING_IO,
        epoc::socket::SOCKET_OPTION_FAMILY_BASE, nullptr, 0));
    REQUIRE(option(epoc::socket::SOCKET_OPTION_ID_NON_BLOCKING_IO) == 1);
    REQUIRE(option(epoc::socket::SOCKET_OPTION_ID_BLOCKING_IO) == 0);

    uv_tcp_t server{}, peer{};
    std::promise<int> listening;
    std::promise<void> accepted;
    struct server_state {
        uv_tcp_t *peer;
        std::promise<void> *accepted;
    } state{&peer, &accepted};
    server.data = &state;
    loop->one_shot([&] {
        uv_tcp_init(loop->raw_loop(), &server);
        sockaddr_in address{};
        uv_ip4_addr("127.0.0.1", 0, &address);
        const int bound = uv_tcp_bind(&server, reinterpret_cast<sockaddr *>(&address), 0);
        const int started = uv_listen(reinterpret_cast<uv_stream_t *>(&server), 1, [](uv_stream_t *server, int error) {
            auto *state = static_cast<server_state *>(server->data);
            uv_tcp_init(server->loop, state->peer);
            uv_accept(server, reinterpret_cast<uv_stream_t *>(state->peer));
            state->accepted->set_value();
        });
        int size = sizeof(address);
        uv_tcp_getsockname(&server, reinterpret_cast<sockaddr *>(&address), &size);
        listening.set_value(bound ? bound : (started ? started : ntohs(address.sin_port)));
    });
    auto ready = listening.get_future();
    REQUIRE(ready.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    const int port = ready.get();
    REQUIRE(port > 0);
    epoc::internet::sinet_address destination{};
    destination.family_ = epoc::internet::INET_ADDRESS_FAMILY;
    destination.port_ = port;
    *destination.addr_long() = 0x7F000001;
    epoc::notify_info notification;
    kernel->lock();
    socket->connect(destination, notification);
    kernel->unlock();
    REQUIRE(accepted.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    REQUIRE(await([&] { return option(epoc::socket::SOCKET_OPTION_ID_SELECT_POLL) & 2; }));

    std::uint8_t data[4]{};
    std::uint32_t received = 99;
    std::int64_t result = 0;
    kernel->lock();
    socket->receive(data, sizeof(data), &received, nullptr, 0, notification,
        [&](std::int64_t count) { result = count; });
    kernel->unlock();
    REQUIRE(result == -1000); // ES_SOCK.H: KErrWouldBlock, with no pending receive.
    REQUIRE(received == 0);

    std::uint32_t events = 0x01000001;
    kernel->lock();
    socket->ioctl(1, notification, reinterpret_cast<std::uint8_t *>(&events), sizeof(events), sizeof(events), 1);
    kernel->unlock();
    uv_write_t write{};
    char payload[] = "test";
    uv_buf_t buffer = uv_buf_init(payload, 4);
    std::promise<void> written;
    write.data = &written;
    loop->one_shot([&] {
        uv_write(&write, reinterpret_cast<uv_stream_t *>(&peer), &buffer, 1, [](uv_write_t *write, int) {
            static_cast<std::promise<void> *>(write->data)->set_value();
        });
    });
    REQUIRE(written.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    REQUIRE(await([&] {
        const std::lock_guard<kernel_system> guard(*kernel);
        return events == 1;
    }));
    REQUIRE(option(epoc::socket::SOCKET_OPTION_ID_READ_BYTES_PENDING) == 4);
    std::promise<std::int64_t> read;
    kernel->lock();
    socket->receive(data, sizeof(data), &received, nullptr, 0, notification,
        [&](std::int64_t count) { read.set_value(count); });
    kernel->unlock();
    auto read_done = read.get_future();
    REQUIRE(read_done.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    REQUIRE(read_done.get() == 4);
    REQUIRE(std::memcmp(data, payload, 4) == 0);
    REQUIRE(option(epoc::socket::SOCKET_OPTION_ID_READ_BYTES_PENDING) == 0);

    kernel->lock();
    events = 1;
    socket->ioctl(1, notification, reinterpret_cast<std::uint8_t *>(&events), sizeof(events), sizeof(events), 1);
    socket->cancel_ioctl();
    socket->ioctl(1, notification, reinterpret_cast<std::uint8_t *>(&events), sizeof(events), sizeof(events), 1);
    kernel->unlock();
    std::promise<void> registered;
    loop->one_shot([&] { registered.set_value(); });
    REQUIRE(registered.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    kernel->lock();
    socket.reset();
    kernel->unlock();
    std::promise<void> closed;
    loop->one_shot([&] {
        uv_close(reinterpret_cast<uv_handle_t *>(&peer), nullptr);
        server.data = &closed;
        uv_close(reinterpret_cast<uv_handle_t *>(&server), [](uv_handle_t *server) {
            static_cast<std::promise<void> *>(server->data)->set_value();
        });
    });
    REQUIRE(closed.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready);

    kernel->lock();
    socket = protocol.make_socket(epoc::internet::INET_ADDRESS_FAMILY,
        epoc::internet::INET_TCP_PROTOCOL_ID, epoc::socket::socket_type_stream);
    REQUIRE(socket->set_option(epoc::socket::SOCKET_OPTION_ID_NON_BLOCKING_IO,
        epoc::socket::SOCKET_OPTION_FAMILY_BASE, nullptr, 0));
    socket->connect(destination, notification);
    kernel->unlock();
    REQUIRE(await([&] { return option(epoc::socket::SOCKET_OPTION_ID_SELECT_POLL) & 4; }));
    REQUIRE(static_cast<std::int32_t>(option(epoc::socket::SOCKET_OPTION_ID_SELECT_LAST_ERROR)) == epoc::error_server_busy);
    kernel->lock();
    socket->connect(destination, notification);
    kernel->unlock();
    REQUIRE(option(epoc::socket::SOCKET_OPTION_ID_SELECT_LAST_ERROR) == 0);
    kernel->lock();
    socket->connect(destination, notification);
    kernel->unlock();
    REQUIRE(await([&] { return option(epoc::socket::SOCKET_OPTION_ID_SELECT_POLL) & 4; }));
    kernel->lock();
    socket.reset();
    kernel->unlock();
}
