#include <services/bluetooth/protocols/bonjour.h>
#include <services/internet/protocols/inet.h>
#include <common/log.h>

#include <CommonCrypto/CommonDigest.h>
#include <dns_sd.h>
#include <uv.h>

#include <array>
#include <cstring>
#include <cstdio>
#include <map>
#include <tuple>

namespace eka2l1::epoc::bt {
    namespace {
        constexpr const char *service_type = "_eka2l1._udp";
        constexpr std::size_t max_services = 64;

        void release(DNSServiceRef &ref) {
            if (ref) {
                DNSServiceRefDeallocate(ref);
                ref = nullptr;
            }
        }
    }

    struct bonjour_discovery::impl {
        struct service {
            impl *owner;
            DNSServiceRef resolve = nullptr;
            DNSServiceRef addresses = nullptr;
            std::uint16_t port = 0;
            device_address address{};
            std::map<std::uint32_t, epoc::socket::saddress> endpoints;

            explicit service(impl *owner) : owner(owner) {}
            ~service() {
                release(addresses);
                release(resolve);
            }
        };

        using service_key = std::tuple<std::uint32_t, std::string, std::string>;
        std::map<service_key, std::unique_ptr<service>> services;
        DNSServiceRef connection = nullptr;
        DNSServiceRef registration = nullptr;
        DNSServiceRef browser = nullptr;
        uv_poll_t *poll = nullptr;
        std::string name;
        device_address address;
        std::function<void()> changed;
        std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> room;
        std::uint16_t port;
        bool failed = false;

        impl(const device_address &address, const std::string &password, std::uint16_t port, std::function<void()> changed)
            : address(address), changed(std::move(changed)), port(port) {
            char instance[32];
            std::snprintf(instance, sizeof(instance), "EKA2L1-%02x%02x%02x%02x%02x%02x",
                address.addr_[0], address.addr_[1], address.addr_[2], address.addr_[3], address.addr_[4], address.addr_[5]);
            name = instance;
            CC_SHA256(password.data(), static_cast<CC_LONG>(password.size()), room.data());
            start();
        }

        ~impl() { stop(); }

        void error(const char *operation, int code) {
            if (!failed) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Bonjour {} failed: {}", operation, code);
            }
            failed = true;
        }

        void stop() {
            if (poll) {
                uv_poll_stop(poll);
                poll->data = nullptr;
                uv_close(reinterpret_cast<uv_handle_t *>(poll), [](uv_handle_t *handle) {
                    delete reinterpret_cast<uv_poll_t *>(handle);
                });
                poll = nullptr;
            }
            // Children must be released before their shared DNS-SD connection.
            services.clear();
            release(browser);
            release(registration);
            release(connection);
        }

        void start() {
            failed = false;
            int result = DNSServiceCreateConnection(&connection);
            if (result) {
                error("connection", result);
                return;
            }

            TXTRecordRef txt;
            TXTRecordCreate(&txt, 0, nullptr);
            TXTRecordSetValue(&txt, "version", 1, "1");
            TXTRecordSetValue(&txt, "room", room.size(), room.data());
            TXTRecordSetValue(&txt, "address", 6, address.addr_);
            registration = connection;
            result = DNSServiceRegister(&registration,
                kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename,
                kDNSServiceInterfaceIndexAny, name.c_str(), service_type, "local.", nullptr,
                htons(port), TXTRecordGetLength(&txt), TXTRecordGetBytesPtr(&txt),
                [](DNSServiceRef, DNSServiceFlags, DNSServiceErrorType error,
                    const char *, const char *, const char *, void *context) {
                    if (error) static_cast<impl *>(context)->error("registration", error);
                }, this);
            TXTRecordDeallocate(&txt);
            if (result) {
                registration = nullptr;
                error("registration", result);
                return;
            }

            browser = connection;
            result = DNSServiceBrowse(&browser, kDNSServiceFlagsShareConnection,
                kDNSServiceInterfaceIndexAny, service_type, "local.", browse_reply, this);
            if (result) {
                browser = nullptr;
                error("browse", result);
                return;
            }

            auto *handle = new uv_poll_t;
            result = uv_poll_init_socket(uv_default_loop(), handle, DNSServiceRefSockFD(connection));
            if (result) {
                delete handle;
                error("poll setup", result);
                return;
            }
            poll = handle;
            poll->data = this;
            result = uv_poll_start(poll, UV_READABLE, [](uv_poll_t *handle, int status, int events) {
                auto *self = static_cast<impl *>(handle->data);
                if (!self) return;
                if (status < 0) {
                    self->error("poll", status);
                    uv_poll_stop(handle);
                } else if (events & UV_READABLE) {
                    const int result = DNSServiceProcessResult(self->connection);
                    if (result) {
                        self->error("receive", result);
                        uv_poll_stop(handle);
                    }
                    self->changed();
                }
            });
            if (result) error("poll start", result);
        }

        static void browse_reply(DNSServiceRef, DNSServiceFlags flags, std::uint32_t interface,
            DNSServiceErrorType error, const char *name, const char *type, const char *domain, void *context) {
            auto *self = static_cast<impl *>(context);
            if (error) {
                self->error("browse", error);
                return;
            }
            if (self->name == name) return;
            const service_key key{interface, name, domain};
            if (!(flags & kDNSServiceFlagsAdd)) {
                self->services.erase(key);
                return;
            }
            if (self->services.count(key) || self->services.size() >= max_services) return;

            auto peer = std::make_unique<service>(self);
            peer->resolve = self->connection;
            const int result = DNSServiceResolve(&peer->resolve, kDNSServiceFlagsShareConnection,
                interface, name, type, domain, resolve_reply, peer.get());
            if (result) {
                peer->resolve = nullptr;
                LOG_ERROR(SERVICE_BLUETOOTH, "Bonjour resolve failed: {}", result);
                return;
            }
            self->services.emplace(key, std::move(peer));
        }

        static void resolve_reply(DNSServiceRef, DNSServiceFlags, std::uint32_t interface,
            DNSServiceErrorType error, const char *, const char *host, std::uint16_t port,
            std::uint16_t txt_size, const unsigned char *txt, void *context) {
            auto *peer = static_cast<service *>(context);
            peer->endpoints.clear();
            release(peer->addresses);
            if (error) return;
            std::uint8_t length = 0;
            const void *version = TXTRecordGetValuePtr(txt_size, txt, "version", &length);
            if (!version || length != 1 || std::memcmp(version, "1", 1)) return;
            const void *room = TXTRecordGetValuePtr(txt_size, txt, "room", &length);
            if (!room || length != peer->owner->room.size()
                || std::memcmp(room, peer->owner->room.data(), length) || !port) return;

            const void *address = TXTRecordGetValuePtr(txt_size, txt, "address", &length);
            if (!address || length != 6) return;
            std::memcpy(peer->address.addr_, address, 6);
            peer->port = ntohs(port);
            peer->addresses = peer->owner->connection;
            const int result = DNSServiceGetAddrInfo(&peer->addresses, kDNSServiceFlagsShareConnection,
                interface, kDNSServiceProtocol_IPv4, host, address_reply, peer);
            if (result) {
                peer->addresses = nullptr;
                LOG_ERROR(SERVICE_BLUETOOTH, "Bonjour address lookup failed: {}", result);
            }
        }

        static void address_reply(DNSServiceRef, DNSServiceFlags flags, std::uint32_t,
            DNSServiceErrorType error, const char *, const sockaddr *address, std::uint32_t, void *context) {
            auto *peer = static_cast<service *>(context);
            if (error || !address || address->sa_family != AF_INET) return;
            const auto key = reinterpret_cast<const sockaddr_in *>(address)->sin_addr.s_addr;
            if (!(flags & kDNSServiceFlagsAdd)) {
                peer->endpoints.erase(key);
                return;
            }
            epoc::socket::saddress endpoint{};
            internet::host_sockaddr_to_guest_saddress(address, endpoint);
            endpoint.port_ = peer->port;
            peer->endpoints[key] = endpoint;
        }
    };

    bonjour_discovery::bonjour_discovery(const device_address &address, const std::string &password, std::uint16_t port, std::function<void()> changed)
        : impl_(std::make_unique<impl>(address, password, port, std::move(changed))) {}

    bonjour_discovery::~bonjour_discovery() = default;

    void bonjour_discovery::retry() {
        if (impl_->failed) {
            impl_->stop();
            impl_->start();
        }
    }

    std::vector<bonjour_peer> bonjour_discovery::peers() const {
        std::vector<bonjour_peer> result;
        if (!impl_->failed) {
            for (const auto &entry : impl_->services) {
                for (const auto &address : entry.second->endpoints) result.push_back({address.second, entry.second->address});
            }
        }
        return result;
    }
}
