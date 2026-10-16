// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch.hpp>

#include <config/app_settings.h>
#include <config/config.h>
#include <kernel/kernel.h>
#include <services/internet/protocols/inet.h>
#include <system/epoc.h>

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
