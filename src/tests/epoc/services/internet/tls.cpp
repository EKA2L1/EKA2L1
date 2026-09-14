// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch.hpp>
#include <drivers/network/tls.h>
#include <dispatch/tls.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>

#include <algorithm>
#include <array>
#include <deque>
#include <fstream>
#include <iterator>

namespace {
    using eka2l1::drivers::tls_session;
    using eka2l1::drivers::tls_result;
    constexpr int want_read = static_cast<int>(tls_result::want_read);
    constexpr int want_write = static_cast<int>(tls_result::want_write);

    std::string fixture(const char *name) {
        std::ifstream stream(std::string("tlsassets/") + name);
        REQUIRE(stream.good());
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    struct peer {
        mbedtls_ssl_context ssl;
        mbedtls_ssl_config config;
        mbedtls_x509_crt cert;
        mbedtls_pk_context key;
        mbedtls_entropy_context entropy;
        mbedtls_ctr_drbg_context rng;
        std::deque<unsigned char> incoming, outgoing;
        bool ready = false;

        explicit peer(mbedtls_ssl_protocol_version version) {
            mbedtls_ssl_init(&ssl);
            mbedtls_ssl_config_init(&config);
            mbedtls_x509_crt_init(&cert);
            mbedtls_pk_init(&key);
            mbedtls_entropy_init(&entropy);
            mbedtls_ctr_drbg_init(&rng);
            REQUIRE(mbedtls_ctr_drbg_seed(&rng, mbedtls_entropy_func, &entropy, nullptr, 0) == 0);
            const auto pem = fixture("server.pem"), private_key = fixture("server-key.pem");
            REQUIRE(mbedtls_x509_crt_parse(&cert, reinterpret_cast<const unsigned char *>(pem.c_str()), pem.size() + 1) == 0);
            REQUIRE(mbedtls_pk_parse_key(&key, reinterpret_cast<const unsigned char *>(private_key.c_str()), private_key.size() + 1,
                nullptr, 0, mbedtls_ctr_drbg_random, &rng) == 0);
            REQUIRE(mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) == 0);
            mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &rng);
            mbedtls_ssl_conf_min_tls_version(&config, version);
            mbedtls_ssl_conf_max_tls_version(&config, version);
            REQUIRE(mbedtls_ssl_conf_own_cert(&config, &cert, &key) == 0);
            REQUIRE(mbedtls_ssl_setup(&ssl, &config) == 0);
            mbedtls_ssl_set_bio(&ssl, this, send, recv, nullptr);
        }
        ~peer() {
            mbedtls_ssl_free(&ssl);
            mbedtls_ssl_config_free(&config);
            mbedtls_x509_crt_free(&cert);
            mbedtls_pk_free(&key);
            mbedtls_ctr_drbg_free(&rng);
            mbedtls_entropy_free(&entropy);
        }
        static int send(void *ctx, const unsigned char *data, size_t size) {
            auto &self = *static_cast<peer *>(ctx);
            self.outgoing.insert(self.outgoing.end(), data, data + size);
            return static_cast<int>(size);
        }
        static int recv(void *ctx, unsigned char *data, size_t size) {
            auto &self = *static_cast<peer *>(ctx);
            if (self.incoming.empty()) return MBEDTLS_ERR_SSL_WANT_READ;
            const auto count = std::min(size, self.incoming.size());
            std::copy_n(self.incoming.begin(), count, data);
            self.incoming.erase(self.incoming.begin(), self.incoming.begin() + count);
            return static_cast<int>(count);
        }
        void transfer(tls_session &client) {
            std::array<unsigned char, 13> data{};
            const auto sent = client.drain(data.data(), data.size());
            incoming.insert(incoming.end(), data.begin(), data.begin() + sent);
            const auto count = std::min<size_t>(7, outgoing.size());
            std::copy_n(outgoing.begin(), count, data.data());
            outgoing.erase(outgoing.begin(), outgoing.begin() + count);
            REQUIRE(client.feed(data.data(), count));
        }
        int handshake(tls_session &client) {
            int result = want_read;
            bool client_ready = false;
            for (int attempt = 0; attempt < 20000; ++attempt) {
                transfer(client);
                if (!client_ready) {
                    result = client.handshake();
                    if (result == 0) client_ready = true;
                    else if (result != want_read && result != want_write) return result;
                }
                if (!ready) {
                    const int server_result = mbedtls_ssl_handshake(&ssl);
                    REQUIRE((server_result == 0 || server_result == MBEDTLS_ERR_SSL_WANT_READ || server_result == MBEDTLS_ERR_SSL_WANT_WRITE));
                    ready = server_result == 0;
                }
                if (ready && client_ready && !client.pending_output() && outgoing.empty()) return 0;
            }
            FAIL("TLS handshake did not finish");
            return result;
        }
    };
}

TEST_CASE("Host TLS authenticates modern certificates across fragmented records", "[tls]") {
    const auto version = GENERATE(MBEDTLS_SSL_VERSION_TLS1_2, MBEDTLS_SSL_VERSION_TLS1_3);
    tls_session client;
    REQUIRE(client.configure("arena.test", fixture("server.pem")));
    peer server(version);
    REQUIRE(server.handshake(client) == 0);
    REQUIRE(client.protocol() == (version == MBEDTLS_SSL_VERSION_TLS1_3 ? "TLSv1.3" : "TLSv1.2"));
    REQUIRE(client.peer_certificate().size() > 0);

    const std::string payload(40000, 'x');
    size_t sent = 0;
    std::string received;
    std::array<unsigned char, 1024> buffer{};
    for (int attempt = 0; attempt < 20000 && received.size() < payload.size(); ++attempt) {
        if (sent < payload.size()) {
            const int result = client.write(reinterpret_cast<const unsigned char *>(payload.data()) + sent, std::min<size_t>(16384, payload.size() - sent));
            REQUIRE(result > 0);
            sent += result;
        }
        server.transfer(client);
        const int result = mbedtls_ssl_read(&server.ssl, buffer.data(), buffer.size());
        REQUIRE((result > 0 || result == MBEDTLS_ERR_SSL_WANT_READ));
        if (result > 0) received.append(reinterpret_cast<char *>(buffer.data()), result);
    }
    REQUIRE(received == payload);

    const unsigned char response[] = "authenticated reply";
    REQUIRE(mbedtls_ssl_write(&server.ssl, response, sizeof(response)) == sizeof(response));
    int result = want_read;
    for (int attempt = 0; attempt < 2000 && result == want_read; ++attempt) {
        server.transfer(client);
        result = client.read(buffer.data(), buffer.size());
    }
    REQUIRE(result == sizeof(response));
    REQUIRE(std::equal(std::begin(response), std::end(response), buffer.begin()));

    REQUIRE(mbedtls_ssl_write(&server.ssl, response, sizeof(response)) == sizeof(response));
    REQUIRE(!server.outgoing.empty());
    server.outgoing.back() ^= 1;
    result = want_read;
    for (int attempt = 0; attempt < 2000 && result == want_read; ++attempt) {
        server.transfer(client);
        result = client.read(buffer.data(), buffer.size());
    }
    REQUIRE(result == static_cast<int>(tls_result::failed));
    REQUIRE(client.write(response, sizeof(response)) == static_cast<int>(tls_result::invalid_state));
}

TEST_CASE("Host TLS rejects a mismatched identity or untrusted issuer", "[tls]") {
    const bool wrong_name = GENERATE(false, true);
    tls_session client;
    REQUIRE(client.configure(wrong_name ? "wrong.test" : "arena.test", fixture(wrong_name ? "server.pem" : "untrusted.pem")));
    peer server(MBEDTLS_SSL_VERSION_TLS1_3);
    REQUIRE(server.handshake(client) == static_cast<int>(tls_result::verification_failed));
    REQUIRE(client.peer_certificate().empty());
}

TEST_CASE("TLS handles belong to their creating process and expire on exit", "[tls]") {
    eka2l1::dispatch::tls_controller controller;
    const int first = controller.create(10, "arena.test", fixture("server.pem"));
    const int second = controller.create(20, "arena.test", fixture("server.pem"));
    REQUIRE(first > 0);
    REQUIRE(second > first);
    REQUIRE(controller.get(20, first) == nullptr);
    REQUIRE_FALSE(controller.erase(20, first));
    REQUIRE(controller.get(10, first) != nullptr);
    controller.erase_process(10);
    REQUIRE(controller.get(10, first) == nullptr);
    REQUIRE(controller.get(20, second) != nullptr);
    REQUIRE(controller.erase(20, second));
    REQUIRE_FALSE(controller.erase(20, second));
}
