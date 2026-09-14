// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later

#include <drivers/network/tls.h>
#include "tls_trust.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>

#include <algorithm>
#include <deque>
#include <mutex>

namespace eka2l1::drivers {
    namespace {
        bool local_ipv4(const unsigned char *bytes) {
            return bytes[0] == 127 || bytes[0] == 10
                || (bytes[0] == 172 && (bytes[1] & 0xf0) == 16)
                || (bytes[0] == 192 && bytes[1] == 168)
                || (bytes[0] == 169 && bytes[1] == 254);
        }

        bool numeric_address(const std::string &address) {
            unsigned char bytes[16];
            return address.find('\0') == std::string::npos
                && (inet_pton(AF_INET, address.c_str(), bytes) == 1
                    || inet_pton(AF_INET6, address.c_str(), bytes) == 1);
        }
    }

    bool is_local_tls_address(const std::string &address) {
        if (address.find('\0') != std::string::npos) return false;
        unsigned char bytes[16]{};
        if (inet_pton(AF_INET, address.c_str(), bytes) == 1) return local_ipv4(bytes);
        if (inet_pton(AF_INET6, address.c_str(), bytes) != 1) return false;
        const unsigned char mapped_prefix[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
        if (std::equal(std::begin(mapped_prefix), std::end(mapped_prefix), bytes)) return local_ipv4(bytes + 12);
        return (std::all_of(bytes, bytes + 15, [](unsigned char b) { return b == 0; }) && bytes[15] == 1)
            || (bytes[0] & 0xfe) == 0xfc || (bytes[0] == 0xfe && (bytes[1] & 0xc0) == 0x80);
    }

    struct tls_session::implementation {
        static constexpr std::size_t queue_limit = 1024 * 1024;
        mbedtls_ssl_context ssl;
        mbedtls_ssl_config config;
        std::string hostname;
        bool local_peer = false;
        mbedtls_ctr_drbg_context random;
        mbedtls_entropy_context entropy;
        std::deque<std::uint8_t> incoming;
        std::deque<std::uint8_t> outgoing;
        bool configured = false;
        bool ready = false;
        bool eof = false;
        bool failed = false;
        int last_error = 0;

        implementation() {
            mbedtls_ssl_init(&ssl);
            mbedtls_ssl_config_init(&config);
            mbedtls_ctr_drbg_init(&random);
            mbedtls_entropy_init(&entropy);
        }

        ~implementation() {
            mbedtls_ssl_free(&ssl);
            mbedtls_ssl_config_free(&config);
            mbedtls_ctr_drbg_free(&random);
            mbedtls_entropy_free(&entropy);
        }

        static int send(void *context, const unsigned char *data, std::size_t size) {
            auto &self = *static_cast<implementation *>(context);
            if (size > queue_limit - self.outgoing.size()) {
                return MBEDTLS_ERR_SSL_WANT_WRITE;
            }
            self.outgoing.insert(self.outgoing.end(), data, data + size);
            return static_cast<int>(size);
        }

        static int receive(void *context, unsigned char *data, std::size_t size) {
            auto &self = *static_cast<implementation *>(context);
            if (self.incoming.empty()) {
                return self.eof ? 0 : MBEDTLS_ERR_SSL_WANT_READ;
            }
            const auto count = std::min(size, self.incoming.size());
            std::copy_n(self.incoming.begin(), count, data);
            self.incoming.erase(self.incoming.begin(), self.incoming.begin() + count);
            return static_cast<int>(count);
        }

        int translate(int result) {
            if (result >= 0) {
                return result;
            }
            switch (result) {
            case MBEDTLS_ERR_SSL_WANT_READ:
                return static_cast<int>(tls_result::want_read);
            case MBEDTLS_ERR_SSL_WANT_WRITE:
                return static_cast<int>(tls_result::want_write);
            case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
                ready = false;
                return static_cast<int>(tls_result::closed);
            default:
                last_error = result;
                failed = true;
                ready = false;
                if (result == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED || result == MBEDTLS_ERR_SSL_BAD_CERTIFICATE) {
                    return static_cast<int>(tls_result::verification_failed);
                }
                return static_cast<int>(tls_result::failed);
            }
        }
    };

    tls_session::tls_session() : impl_(std::make_unique<implementation>()) {}
    tls_session::~tls_session() = default;

    bool tls_session::configure(const std::string &hostname, const std::string &peer_address) {
        if (impl_->configured || hostname.empty() || hostname.find('\0') != std::string::npos || !numeric_address(peer_address)) {
            return false;
        }
        impl_ = std::make_unique<implementation>();
        static std::once_flag initialized;
        static psa_status_t crypto_status;
        std::call_once(initialized, [] { crypto_status = psa_crypto_init(); });
        if (crypto_status != PSA_SUCCESS) {
            return false;
        }
        auto &self = *impl_;
        self.hostname = hostname;
        self.local_peer = is_local_tls_address(peer_address);
        static const unsigned char personalization[] = "EKA2L1 TLS";
        int result = mbedtls_ctr_drbg_seed(&self.random, mbedtls_entropy_func, &self.entropy,
            personalization, sizeof(personalization) - 1);
        if (result == 0) {
            result = mbedtls_ssl_config_defaults(&self.config, MBEDTLS_SSL_IS_CLIENT,
                MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
        }
        if (result != 0) {
            self.last_error = result;
            return false;
        }
        // The platform validates the peer before any application data is released.
        mbedtls_ssl_conf_authmode(&self.config, MBEDTLS_SSL_VERIFY_OPTIONAL);
        mbedtls_ssl_conf_min_tls_version(&self.config, MBEDTLS_SSL_VERSION_TLS1_2);
        mbedtls_ssl_conf_rng(&self.config, mbedtls_ctr_drbg_random, &self.random);
        result = mbedtls_ssl_setup(&self.ssl, &self.config);
        if (result == 0) {
            result = mbedtls_ssl_set_hostname(&self.ssl, hostname.c_str());
        }
        if (result != 0) {
            self.last_error = result;
            return false;
        }
        mbedtls_ssl_set_bio(&self.ssl, &self, implementation::send, implementation::receive, nullptr);
        self.configured = true;
        return true;
    }

    int tls_session::handshake() {
        if (!impl_->configured || impl_->failed) {
            return static_cast<int>(tls_result::invalid_state);
        }
        const int result = impl_->translate(mbedtls_ssl_handshake(&impl_->ssl));
        if (result == 0 && !impl_->ready) {
            if (!impl_->local_peer) {
                tls_certificate_chain chain;
                for (auto *cert = mbedtls_ssl_get_peer_cert(&impl_->ssl); cert; cert = cert->next) {
                    chain.emplace_back(cert->raw.p, cert->raw.p + cert->raw.len);
                }
                const auto flags = mbedtls_ssl_get_verify_result(&impl_->ssl);
                if ((flags & MBEDTLS_X509_BADCERT_CN_MISMATCH) || !verify_system_certificate(chain, impl_->hostname)) {
                    return impl_->translate(MBEDTLS_ERR_X509_CERT_VERIFY_FAILED);
                }
            }
            impl_->ready = true;
        }
        return result;
    }

    int tls_session::read(std::uint8_t *data, std::size_t size) {
        if (!impl_->ready || (!data && size)) {
            return static_cast<int>(tls_result::invalid_state);
        }
        if (!size) {
            return 0;
        }
        const int result = impl_->translate(mbedtls_ssl_read(&impl_->ssl, data, size));
        if (result == 0) {
            impl_->ready = false;
            return static_cast<int>(tls_result::closed);
        }
        return result;
    }

    int tls_session::write(const std::uint8_t *data, std::size_t size) {
        if (!impl_->ready || (!data && size)) {
            return static_cast<int>(tls_result::invalid_state);
        }
        return impl_->translate(mbedtls_ssl_write(&impl_->ssl, data, size));
    }

    int tls_session::close_notify() {
        if (!impl_->ready) {
            return static_cast<int>(tls_result::invalid_state);
        }
        const int result = impl_->translate(mbedtls_ssl_close_notify(&impl_->ssl));
        if (result == 0) {
            impl_->ready = false;
        }
        return result;
    }

    bool tls_session::feed(const std::uint8_t *data, std::size_t size) {
        if ((!data && size) || impl_->eof || size > implementation::queue_limit - impl_->incoming.size()) {
            return false;
        }
        if (size) {
            impl_->incoming.insert(impl_->incoming.end(), data, data + size);
        }
        return true;
    }

    void tls_session::transport_closed() { impl_->eof = true; }

    std::size_t tls_session::drain(std::uint8_t *data, std::size_t size) {
        if (!data) {
            return 0;
        }
        const auto count = std::min(size, impl_->outgoing.size());
        std::copy_n(impl_->outgoing.begin(), count, data);
        impl_->outgoing.erase(impl_->outgoing.begin(), impl_->outgoing.begin() + count);
        return count;
    }

    std::size_t tls_session::pending_output() const { return impl_->outgoing.size(); }
    std::string tls_session::protocol() const { return impl_->ready ? mbedtls_ssl_get_version(&impl_->ssl) : ""; }

    std::uint16_t tls_session::cipher_suite() const {
        return impl_->ready ? static_cast<std::uint16_t>(mbedtls_ssl_get_ciphersuite_id(mbedtls_ssl_get_ciphersuite(&impl_->ssl))) : 0;
    }

    std::vector<std::uint8_t> tls_session::peer_certificate() const {
        const auto *cert = impl_->ready ? mbedtls_ssl_get_peer_cert(&impl_->ssl) : nullptr;
        return cert ? std::vector<std::uint8_t>(cert->raw.p, cert->raw.p + cert->raw.len) : std::vector<std::uint8_t>{};
    }

    std::string tls_session::error() const {
        char buffer[256]{};
        mbedtls_strerror(impl_->last_error, buffer, sizeof(buffer));
        return buffer;
    }
}
