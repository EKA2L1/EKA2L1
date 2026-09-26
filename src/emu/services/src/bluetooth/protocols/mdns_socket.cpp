#include "mdns_common.h"

#include <services/bluetooth/protocols/mdns.h>
#include <services/bluetooth/protocols/common_inet.h>
#include <services/internet/protocols/inet.h>
#include <common/log.h>

#include <uv.h>

#ifdef __ANDROID__
#include <common/android/ifaddrs.h>
#include <common/android/jniutils.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <map>

namespace eka2l1::epoc::bt {
    namespace {
        constexpr std::uint16_t MDNS_PORT = 5353;
        constexpr const char *MDNS_GROUP = "224.0.0.251";
        constexpr std::uint32_t RECORD_TTL = 120;
        constexpr std::uint32_t LEGACY_UNICAST_TTL = 10;
        constexpr std::uint64_t ANNOUNCE_REPEAT_MS = 1000;
        constexpr std::uint64_t MAINTENANCE_MS = 10000;
        constexpr std::uint64_t FOLLOW_UP_INTERVAL_MS = 1000;
        constexpr std::size_t MAX_SERVICES = 64;

        constexpr std::uint16_t TYPE_A = 1;
        constexpr std::uint16_t TYPE_PTR = 12;
        constexpr std::uint16_t TYPE_TXT = 16;
        constexpr std::uint16_t TYPE_SRV = 33;
        constexpr std::uint16_t TYPE_ANY = 255;
        constexpr std::uint16_t CLASS_IN = 1;
        constexpr std::uint16_t CLASS_TOP_BIT = 0x8000;
        constexpr std::uint16_t FLAG_RESPONSE = 0x8000;
        constexpr std::uint16_t FLAG_AUTHORITATIVE = 0x0400;

        std::string lowercase(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        bool ends_with(const std::string &value, const std::string &suffix) {
            return (value.size() > suffix.size()) && (value[value.size() - suffix.size() - 1] == '.')
                && (value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0);
        }

        struct packet_reader {
            const std::uint8_t *data;
            std::size_t size;
            std::size_t pos = 0;
            bool ok = true;

            packet_reader(const std::uint8_t *data, std::size_t size) : data(data), size(size) {}

            std::uint16_t u16() {
                if (pos + 2 > size) {
                    ok = false;
                    return 0;
                }
                const std::uint16_t value = static_cast<std::uint16_t>((data[pos] << 8) | data[pos + 1]);
                pos += 2;
                return value;
            }

            std::uint32_t u32() {
                const std::uint32_t high = u16();
                return (high << 16) | u16();
            }

            // Decodes a possibly compressed name into lowercase dotted form.
            std::string name() {
                std::string result;
                std::size_t cursor = pos;
                bool jumped = false;
                int jumps = 0;

                while (true) {
                    if (cursor >= size) {
                        ok = false;
                        return {};
                    }
                    const std::uint8_t length = data[cursor];
                    if ((length & 0xC0) == 0xC0) {
                        if ((cursor + 1 >= size) || (++jumps > 16)) {
                            ok = false;
                            return {};
                        }
                        if (!jumped) pos = cursor + 2;
                        jumped = true;
                        cursor = (static_cast<std::size_t>(length & 0x3F) << 8) | data[cursor + 1];
                        continue;
                    }
                    if (length & 0xC0) {
                        ok = false;
                        return {};
                    }
                    cursor++;
                    if (length == 0) break;
                    if ((cursor + length > size) || (result.size() + length > 255)) {
                        ok = false;
                        return {};
                    }
                    if (!result.empty()) result += '.';
                    result.append(reinterpret_cast<const char *>(data + cursor), length);
                    cursor += length;
                }

                if (!jumped) pos = cursor;
                return lowercase(result);
            }
        };

        struct packet_writer {
            std::vector<std::uint8_t> data;

            void u16(std::uint16_t value) {
                data.push_back(static_cast<std::uint8_t>(value >> 8));
                data.push_back(static_cast<std::uint8_t>(value));
            }

            void u32(std::uint32_t value) {
                u16(static_cast<std::uint16_t>(value >> 16));
                u16(static_cast<std::uint16_t>(value));
            }

            void name(const std::string &dotted) {
                std::size_t start = 0;
                while (start < dotted.size()) {
                    std::size_t end = dotted.find('.', start);
                    if (end == std::string::npos) end = dotted.size();
                    const std::size_t length = std::min<std::size_t>(end - start, 63);
                    data.push_back(static_cast<std::uint8_t>(length));
                    data.insert(data.end(), dotted.begin() + start, dotted.begin() + start + length);
                    start = end + 1;
                }
                data.push_back(0);
            }

            void header(std::uint16_t id, std::uint16_t flags, std::uint16_t questions, std::uint16_t answers) {
                u16(id);
                u16(flags);
                u16(questions);
                u16(answers);
                u16(0);
                u16(0);
            }

            // Returns the offset of the RDLENGTH field for end_record().
            std::size_t begin_record(const std::string &owner, std::uint16_t type, bool unique, std::uint32_t ttl) {
                name(owner);
                u16(type);
                u16(unique ? (CLASS_IN | CLASS_TOP_BIT) : CLASS_IN);
                u32(ttl);
                u16(0);
                return data.size() - 2;
            }

            void end_record(std::size_t length_offset) {
                const std::size_t length = data.size() - length_offset - 2;
                data[length_offset] = static_cast<std::uint8_t>(length >> 8);
                data[length_offset + 1] = static_cast<std::uint8_t>(length);
            }

            void txt_entry(const char *key, const void *value, std::size_t size) {
                const std::size_t key_size = std::strlen(key);
                data.push_back(static_cast<std::uint8_t>(key_size + 1 + size));
                data.insert(data.end(), key, key + key_size);
                data.push_back('=');
                data.insert(data.end(), static_cast<const std::uint8_t *>(value), static_cast<const std::uint8_t *>(value) + size);
            }
        };

        struct question {
            std::string name;
            std::uint16_t type;
        };

        struct interface_address {
            std::uint32_t address; // Network order, like sin_addr.
            std::uint32_t netmask;
        };

        void add_interface(std::vector<interface_address> &result, const interface_address &entry) {
            if (std::none_of(result.begin(), result.end(), [&](const interface_address &other) { return other.address == entry.address; })) {
                result.push_back(entry);
            }
        }

        std::vector<interface_address> list_interfaces() {
            std::vector<interface_address> result;
#ifdef __ANDROID__
            // libuv has no interface enumeration below API 24.
            ifaddrs *addresses = nullptr;
            if (getifaddrs(&addresses) != 0) {
                return result;
            }
            for (const ifaddrs *entry = addresses; entry; entry = entry->ifa_next) {
                if (!entry->ifa_addr || (entry->ifa_addr->sa_family != AF_INET) || (entry->ifa_flags & IFF_LOOPBACK)) {
                    continue;
                }
                const auto *netmask = reinterpret_cast<const sockaddr_in *>(entry->ifa_netmask);
                add_interface(result, { reinterpret_cast<const sockaddr_in *>(entry->ifa_addr)->sin_addr.s_addr,
                    netmask ? netmask->sin_addr.s_addr : 0xFFFFFFFF });
            }
            freeifaddrs(addresses);
#else
            uv_interface_address_t *addresses = nullptr;
            int count = 0;
            if (uv_interface_addresses(&addresses, &count) != 0) {
                return result;
            }
            for (int i = 0; i < count; i++) {
                if (addresses[i].is_internal || (addresses[i].address.address4.sin_family != AF_INET)) {
                    continue;
                }
                add_interface(result, { addresses[i].address.address4.sin_addr.s_addr, addresses[i].netmask.netmask4.sin_addr.s_addr });
            }
            uv_free_interface_addresses(addresses, count);
#endif
            return result;
        }

        std::string ipv4_string(std::uint32_t address) {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = address;
            char buffer[32] = {};
            uv_ip4_name(&addr, buffer, sizeof(buffer));
            return buffer;
        }

        // Android drops Wi-Fi multicast unless the app holds a MulticastLock.
        void set_multicast_lock(bool held) {
#ifdef __ANDROID__
            JNIEnv *env = common::jni::environment();
            if (!env) return;
            jclass clazz = common::jni::find_class("com/github/eka2l1/emu/Emulator");
            jmethodID method = env->GetStaticMethodID(clazz, "setMulticastLock", "(Z)V");
            env->CallStaticVoidMethod(clazz, method, static_cast<jboolean>(held));
            env->DeleteLocalRef(clazz);
#else
            static_cast<void>(held);
#endif
        }
    }

    struct mdns_discovery::impl {
        struct service {
            std::string host;
            std::uint16_t port = 0;
            bool has_srv = false;
            bool has_txt = false;
            bool in_room = false;
            device_address address{};
            std::uint64_t expiry = 0;
            std::uint64_t last_follow_up = 0;
        };

        std::map<std::string, service> services;
        std::map<std::string, std::map<std::uint32_t, std::uint64_t>> hosts;
        std::vector<interface_address> interfaces;
        std::array<std::uint8_t, 9000> receive_buffer;

        uv_udp_t *socket = nullptr;
        uv_timer_t *timer = nullptr;
        bool announced_twice = false;
        bool browse_only = false;
        bool failed = false;

        std::string instance;
        std::string instance_key;
        std::string service_name;
        std::string host_name;
        device_address address;
        std::function<void()> changed;
        mdns::room_digest room;
        std::uint16_t port;

        impl(const device_address &address, const std::string &password, std::uint16_t port, std::function<void()> changed)
            : address(address), changed(std::move(changed)), room(mdns::make_room_digest(password)), port(port) {
            const std::string label = mdns::make_instance_name(address);
            service_name = lowercase(std::string(mdns::SERVICE_TYPE) + ".local");
            instance = label + "." + mdns::SERVICE_TYPE + ".local";
            instance_key = lowercase(instance);
            host_name = label + ".local";
            start();
        }

        ~impl() { stop(); }

        void error(const char *operation, int code) {
            if (!failed) {
                LOG_ERROR(SERVICE_BLUETOOTH, "mDNS {} failed: {}", operation, uv_strerror(code));
            }
            failed = true;
        }

        std::uint64_t now() const {
            return uv_now(uv_default_loop());
        }

        void start() {
            failed = false;
            browse_only = false;
            announced_twice = false;

            interfaces = list_interfaces();
            if (interfaces.empty()) {
                error("interface lookup", UV_ENETDOWN);
                return;
            }

            set_multicast_lock(true);

            socket = new uv_udp_t;
            uv_udp_init(uv_default_loop(), socket);
            socket->data = this;

            sockaddr_in bind_addr{};
            uv_ip4_addr("0.0.0.0", MDNS_PORT, &bind_addr);
            if (const int result = uv_udp_bind(socket, reinterpret_cast<const sockaddr *>(&bind_addr), UV_UDP_REUSEADDR)) {
                // An ephemeral port can still browse through legacy unicast replies,
                // but responses from it are not accepted by other mDNS hosts.
                LOG_WARN(SERVICE_BLUETOOTH, "mDNS can't bind port {} ({}), this device won't be discoverable", MDNS_PORT, uv_strerror(result));
                browse_only = true;
                bind_addr.sin_port = 0;
                if (const int fallback = uv_udp_bind(socket, reinterpret_cast<const sockaddr *>(&bind_addr), 0)) {
                    error("bind", fallback);
                    return;
                }
            }

            uv_udp_set_multicast_ttl(socket, 255);
            uv_udp_set_multicast_loop(socket, 1);
            join_groups();

            if (const int result = uv_udp_recv_start(socket, [](uv_handle_t *handle, std::size_t, uv_buf_t *buf) {
                    auto *self = static_cast<impl *>(handle->data);
                    buf->base = reinterpret_cast<char *>(self->receive_buffer.data());
                    buf->len = static_cast<decltype(buf->len)>(self->receive_buffer.size());
                }, [](uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const sockaddr *sender, unsigned) {
                    auto *self = static_cast<impl *>(handle->data);
                    if (!self) return;
                    if (nread < 0) {
                        if (is_socket_dead_error(static_cast<int>(nread))) {
                            self->error("receive", static_cast<int>(nread));
                            uv_udp_recv_stop(handle);
                        }
                        return;
                    }
                    if ((nread > 0) && sender && (sender->sa_family == AF_INET)) {
                        self->receive(reinterpret_cast<const std::uint8_t *>(buf->base), static_cast<std::size_t>(nread),
                            *reinterpret_cast<const sockaddr_in *>(sender));
                    }
                })) {
                error("receive start", result);
                return;
            }

            timer = new uv_timer_t;
            uv_timer_init(uv_default_loop(), timer);
            timer->data = this;
            uv_timer_start(timer, [](uv_timer_t *handle) {
                static_cast<impl *>(handle->data)->maintain();
            }, ANNOUNCE_REPEAT_MS, MAINTENANCE_MS);

            announce(RECORD_TTL);
            query({ { service_name, TYPE_PTR } });
        }

        void stop() {
            if (socket && !failed) {
                announce(0);
            }
            if (timer) {
                uv_timer_stop(timer);
                timer->data = nullptr;
                uv_close(reinterpret_cast<uv_handle_t *>(timer), [](uv_handle_t *handle) {
                    delete reinterpret_cast<uv_timer_t *>(handle);
                });
                timer = nullptr;
            }
            if (socket) {
                uv_udp_recv_stop(socket);
                socket->data = nullptr;
                uv_close(reinterpret_cast<uv_handle_t *>(socket), [](uv_handle_t *handle) {
                    delete reinterpret_cast<uv_udp_t *>(handle);
                });
                socket = nullptr;
                set_multicast_lock(false);
            }
            services.clear();
            hosts.clear();
        }

        void join_groups() {
            for (const auto &entry : interfaces) {
                // Re-joining a group on an interface reports an error that changes nothing.
                uv_udp_set_membership(socket, MDNS_GROUP, ipv4_string(entry.address).c_str(), UV_JOIN_GROUP);
            }
        }

        void refresh() {
            if (failed) {
                stop();
                start();
                return;
            }
            const auto current = list_interfaces();
            if (current.empty()) {
                error("interface lookup", UV_ENETDOWN);
                return;
            }
            if (current.size() != interfaces.size() || !std::equal(current.begin(), current.end(), interfaces.begin(),
                    [](const interface_address &a, const interface_address &b) { return a.address == b.address; })) {
                interfaces = current;
                join_groups();
                announce(RECORD_TTL);
            }
            query({ { service_name, TYPE_PTR } });
        }

        void send(const std::vector<std::uint8_t> &packet, const sockaddr_in &target) {
            uv_buf_t buf = uv_buf_init(reinterpret_cast<char *>(const_cast<std::uint8_t *>(packet.data())),
                static_cast<unsigned int>(packet.size()));
            // Packets are small, so a synchronous send keeps the per-interface
            // multicast selection paired with its own datagram.
            const int result = uv_udp_try_send(socket, &buf, 1, reinterpret_cast<const sockaddr *>(&target));
            if ((result < 0) && (result != UV_EAGAIN)) {
                LOG_TRACE(SERVICE_BLUETOOTH, "mDNS send failed: {}", uv_strerror(result));
            }
        }

        void send_multicast(const std::function<std::vector<std::uint8_t>(const interface_address &)> &build) {
            sockaddr_in group{};
            uv_ip4_addr(MDNS_GROUP, MDNS_PORT, &group);
            for (const auto &entry : interfaces) {
                if (uv_udp_set_multicast_interface(socket, ipv4_string(entry.address).c_str()) == 0) {
                    send(build(entry), group);
                }
            }
        }

        void write_records(packet_writer &writer, std::uint32_t ttl, bool legacy, std::uint32_t ip) {
            const bool unique = !legacy;
            std::size_t length = writer.begin_record(service_name, TYPE_PTR, false, ttl);
            writer.name(instance);
            writer.end_record(length);

            length = writer.begin_record(instance, TYPE_SRV, unique, ttl);
            writer.u16(0);
            writer.u16(0);
            writer.u16(port);
            writer.name(host_name);
            writer.end_record(length);

            length = writer.begin_record(instance, TYPE_TXT, unique, ttl);
            writer.txt_entry("version", mdns::RECORD_VERSION, 1);
            writer.txt_entry("room", room.data(), room.size());
            writer.txt_entry("address", address.addr_, 6);
            writer.end_record(length);

            length = writer.begin_record(host_name, TYPE_A, unique, ttl);
            writer.data.insert(writer.data.end(), reinterpret_cast<const std::uint8_t *>(&ip), reinterpret_cast<const std::uint8_t *>(&ip) + 4);
            writer.end_record(length);
        }

        std::vector<std::uint8_t> build_response(std::uint16_t id, const std::vector<question> &questions, std::uint32_t ttl,
            bool legacy, std::uint32_t ip) {
            packet_writer writer;
            writer.header(id, FLAG_RESPONSE | FLAG_AUTHORITATIVE, static_cast<std::uint16_t>(questions.size()), 4);
            for (const auto &entry : questions) {
                writer.name(entry.name);
                writer.u16(entry.type);
                writer.u16(CLASS_IN);
            }
            write_records(writer, ttl, legacy, ip);
            return writer.data;
        }

        void announce(std::uint32_t ttl) {
            if (browse_only) return;
            send_multicast([&](const interface_address &entry) {
                return build_response(0, {}, ttl, false, entry.address);
            });
        }

        void query(const std::vector<question> &questions) {
            if (questions.empty()) return;
            packet_writer writer;
            writer.header(0, 0, static_cast<std::uint16_t>(questions.size()), 0);
            for (const auto &entry : questions) {
                writer.name(entry.name);
                writer.u16(entry.type);
                writer.u16(CLASS_IN);
            }
            send_multicast([&](const interface_address &) { return writer.data; });
        }

        bool asks_for_us(const question &entry) const {
            const bool any = (entry.type == TYPE_ANY);
            if (entry.name == service_name) return any || (entry.type == TYPE_PTR);
            if (entry.name == instance_key) return any || (entry.type == TYPE_SRV) || (entry.type == TYPE_TXT);
            if (entry.name == lowercase(host_name)) return any || (entry.type == TYPE_A);
            return false;
        }

        void receive(const std::uint8_t *data, std::size_t size, const sockaddr_in &sender) {
            packet_reader reader(data, size);
            const std::uint16_t id = reader.u16();
            const std::uint16_t flags = reader.u16();
            const std::uint16_t question_count = reader.u16();
            std::uint32_t record_count = reader.u16();
            record_count += reader.u16();
            record_count += reader.u16();
            if (!reader.ok) return;

            std::vector<question> questions;
            bool wants_us = false;
            bool unicast_reply = false;
            for (std::uint16_t i = 0; (i < question_count) && reader.ok; i++) {
                question entry{ reader.name(), reader.u16() };
                const std::uint16_t cls = reader.u16();
                if (!reader.ok) return;
                if ((cls & ~CLASS_TOP_BIT) != CLASS_IN) continue;
                if (asks_for_us(entry)) {
                    wants_us = true;
                    unicast_reply |= (cls & CLASS_TOP_BIT) != 0;
                }
                questions.push_back(std::move(entry));
            }

            if (!(flags & FLAG_RESPONSE)) {
                if (wants_us && !browse_only) respond(id, questions, sender, unicast_reply);
                return;
            }
            if (ntohs(sender.sin_port) != MDNS_PORT) return;

            struct resource {
                std::string owner;
                std::uint16_t type;
                std::uint32_t ttl;
                std::size_t offset;
                std::uint16_t length;
            };
            std::vector<resource> resources;
            for (std::uint32_t i = 0; i < record_count; i++) {
                resource entry{};
                entry.owner = reader.name();
                entry.type = reader.u16();
                const std::uint16_t cls = reader.u16();
                entry.ttl = reader.u32();
                entry.length = reader.u16();
                entry.offset = reader.pos;
                if (!reader.ok || (reader.pos + entry.length > size)) return;
                reader.pos += entry.length;
                if ((cls & ~CLASS_TOP_BIT) == CLASS_IN) resources.push_back(std::move(entry));
            }

            // Addresses are only kept for known SRV targets, which may come later in the packet.
            std::stable_partition(resources.begin(), resources.end(), [](const resource &entry) { return entry.type != TYPE_A; });
            bool updated = false;
            for (const auto &entry : resources) {
                packet_reader rdata(data, entry.offset + entry.length);
                rdata.pos = entry.offset;
                updated |= record(entry.owner, entry.type, entry.ttl, rdata, entry.length);
            }

            follow_up();
            if (updated) changed();
        }

        void respond(std::uint16_t id, const std::vector<question> &questions, const sockaddr_in &sender, bool unicast) {
            const bool legacy = ntohs(sender.sin_port) != MDNS_PORT;
            if (!legacy && !unicast) {
                send_multicast([&](const interface_address &entry) {
                    return build_response(0, {}, RECORD_TTL, false, entry.address);
                });
                return;
            }

            if (interfaces.empty()) return;
            std::uint32_t ip = interfaces.front().address;
            for (const auto &entry : interfaces) {
                if ((entry.address & entry.netmask) == (sender.sin_addr.s_addr & entry.netmask)) {
                    ip = entry.address;
                    break;
                }
            }
            send(build_response(legacy ? id : 0, legacy ? questions : std::vector<question>{},
                legacy ? LEGACY_UNICAST_TTL : RECORD_TTL, legacy, ip), sender);
        }

        service *find_service(const std::string &owner, bool create) {
            if ((owner == instance_key) || !ends_with(owner, service_name)) return nullptr;
            auto found = services.find(owner);
            if (found != services.end()) return &found->second;
            if (!create || (services.size() >= MAX_SERVICES)) return nullptr;
            return &services[owner];
        }

        bool record(const std::string &owner, std::uint16_t type, std::uint32_t ttl, packet_reader &rdata, std::uint16_t length) {
            const std::uint64_t expiry = now() + static_cast<std::uint64_t>(ttl) * 1000;

            switch (type) {
            case TYPE_PTR: {
                if (owner != service_name) return false;
                const std::string target = rdata.name();
                if (!rdata.ok) return false;
                if (ttl == 0) return services.erase(target) != 0;
                if (service *peer = find_service(target, true)) {
                    peer->expiry = std::max(peer->expiry, expiry);
                }
                return false;
            }

            case TYPE_SRV: {
                rdata.u16();
                rdata.u16();
                const std::uint16_t target_port = rdata.u16();
                const std::string target = rdata.name();
                if (!rdata.ok) return false;
                if (ttl == 0) return services.erase(owner) != 0;
                service *peer = find_service(owner, true);
                if (!peer) return false;
                const bool updated = !peer->has_srv || (peer->port != target_port) || (peer->host != target);
                peer->has_srv = true;
                peer->port = target_port;
                peer->host = target;
                peer->expiry = std::max(peer->expiry, expiry);
                return updated;
            }

            case TYPE_TXT: {
                service *peer = find_service(owner, ttl != 0);
                if (!peer) return false;
                if (ttl == 0) return services.erase(owner) != 0;
                bool version = false, in_room = false, has_address = false;
                device_address found_address{};
                const std::size_t end = rdata.pos + length;
                while (rdata.pos < end) {
                    const std::size_t entry_size = rdata.data[rdata.pos++];
                    if (rdata.pos + entry_size > end) break;
                    const char *entry = reinterpret_cast<const char *>(rdata.data + rdata.pos);
                    const char *separator = static_cast<const char *>(std::memchr(entry, '=', entry_size));
                    rdata.pos += entry_size;
                    if (!separator) continue;
                    const std::string key = lowercase(std::string(entry, separator));
                    const std::size_t value_size = entry_size - (separator - entry) - 1;
                    const char *value = separator + 1;
                    if (key == "version") {
                        version = (value_size == 1) && (std::memcmp(value, mdns::RECORD_VERSION, 1) == 0);
                    } else if (key == "room") {
                        in_room = (value_size == room.size()) && (std::memcmp(value, room.data(), room.size()) == 0);
                    } else if ((key == "address") && (value_size == 6)) {
                        std::memcpy(found_address.addr_, value, 6);
                        has_address = true;
                    }
                }
                const bool matches = version && in_room && has_address;
                const bool updated = !peer->has_txt || (peer->in_room != matches)
                    || (std::memcmp(peer->address.addr_, found_address.addr_, 6) != 0);
                peer->has_txt = true;
                peer->in_room = matches;
                peer->address = found_address;
                peer->expiry = std::max(peer->expiry, expiry);
                return updated;
            }

            case TYPE_A: {
                if (length != 4) return false;
                std::uint32_t ip = 0;
                std::memcpy(&ip, rdata.data + rdata.pos, 4);
                const bool known = std::any_of(services.begin(), services.end(), [&](const auto &entry) {
                    return entry.second.host == owner;
                });
                if (ttl == 0) {
                    auto host = hosts.find(owner);
                    return (host != hosts.end()) && (host->second.erase(ip) != 0);
                }
                if (!known) return false;
                auto &addresses = hosts[owner];
                const bool updated = addresses.count(ip) == 0;
                addresses[ip] = expiry;
                return updated;
            }

            default:
                return false;
            }
        }

        // Some responders answer a PTR question without the SRV, TXT or A records.
        void follow_up() {
            const std::uint64_t current = now();
            std::vector<question> questions;
            for (auto &entry : services) {
                service &peer = entry.second;
                const bool needs_host = peer.has_srv && peer.in_room && hosts[peer.host].empty();
                if ((peer.has_srv && peer.has_txt && !needs_host) || (current - peer.last_follow_up < FOLLOW_UP_INTERVAL_MS)) {
                    continue;
                }
                peer.last_follow_up = current;
                if (!peer.has_srv) questions.push_back({ entry.first, TYPE_SRV });
                if (!peer.has_txt) questions.push_back({ entry.first, TYPE_TXT });
                if (needs_host) questions.push_back({ peer.host, TYPE_A });
            }
            query(questions);
        }

        void maintain() {
            if (!announced_twice) {
                announced_twice = true;
                announce(RECORD_TTL);
                query({ { service_name, TYPE_PTR } });
                return;
            }

            const std::uint64_t current = now();
            const std::uint64_t refresh_window = RECORD_TTL * 1000 / 4;
            bool removed = false;
            bool refresh_needed = false;

            for (auto entry = services.begin(); entry != services.end();) {
                if (entry->second.expiry <= current) {
                    entry = services.erase(entry);
                    removed = true;
                } else {
                    refresh_needed |= entry->second.expiry - current < refresh_window;
                    ++entry;
                }
            }
            for (auto host = hosts.begin(); host != hosts.end();) {
                for (auto ip = host->second.begin(); ip != host->second.end();) {
                    if (ip->second <= current) {
                        ip = host->second.erase(ip);
                        removed = true;
                    } else {
                        refresh_needed |= ip->second - current < refresh_window;
                        ++ip;
                    }
                }
                const bool used = std::any_of(services.begin(), services.end(), [&](const auto &entry) {
                    return entry.second.host == host->first;
                });
                host = used ? std::next(host) : hosts.erase(host);
            }

            if (refresh_needed) query({ { service_name, TYPE_PTR } });
            if (removed) changed();
        }

        std::vector<mdns_peer> peers() const {
            std::vector<mdns_peer> result;
            if (failed) return result;
            const std::uint64_t current = now();
            for (const auto &entry : services) {
                const service &peer = entry.second;
                if (!peer.has_srv || !peer.in_room || !peer.port || (peer.expiry <= current)) continue;
                const auto host = hosts.find(peer.host);
                if (host == hosts.end()) continue;
                for (const auto &ip : host->second) {
                    if (ip.second <= current) continue;
                    sockaddr_in addr{};
                    addr.sin_family = AF_INET;
                    addr.sin_addr.s_addr = ip.first;
                    mdns_peer found{};
                    internet::host_sockaddr_to_guest_saddress(reinterpret_cast<const sockaddr *>(&addr), found.endpoint);
                    found.endpoint.port_ = peer.port;
                    found.address = peer.address;
                    result.push_back(found);
                }
            }
            return result;
        }
    };

    mdns_discovery::mdns_discovery(const device_address &address, const std::string &password, std::uint16_t port, std::function<void()> changed)
        : impl_(std::make_unique<impl>(address, password, port, std::move(changed))) {}

    mdns_discovery::~mdns_discovery() = default;

    void mdns_discovery::refresh() {
        impl_->refresh();
    }

    std::vector<mdns_peer> mdns_discovery::peers() const {
        return impl_->peers();
    }
}
