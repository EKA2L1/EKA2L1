// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace eka2l1::drivers {
    enum class tls_result : int {
        want_read = -30000,
        want_write = -30001,
        closed = -30002,
        verification_failed = -30003,
        invalid_state = -30004,
        failed = -30005
    };

    bool is_local_tls_address(const std::string &address);

    // Serialized, nonblocking TLS over caller-owned byte streams; never opens a socket.
    class tls_session {
        struct implementation;
        std::unique_ptr<implementation> impl_;

    public:
        tls_session();
        ~tls_session();
        tls_session(const tls_session &) = delete;
        tls_session &operator=(const tls_session &) = delete;

        bool configure(const std::string &hostname, const std::string &peer_address);
        int handshake();
        int read(std::uint8_t *data, std::size_t size);
        int write(const std::uint8_t *data, std::size_t size);
        int close_notify();
        bool feed(const std::uint8_t *data, std::size_t size);
        void transport_closed();
        std::size_t drain(std::uint8_t *data, std::size_t size);
        std::size_t pending_output() const;
        std::string protocol() const;
        std::uint16_t cipher_suite() const;
        std::vector<std::uint8_t> peer_certificate() const;
        std::string error() const;
    };
}
