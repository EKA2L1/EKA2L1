#pragma once

#include <services/bluetooth/protocols/common.h>

#include <mbedtls/sha256.h>

#include <array>
#include <cstdio>
#include <string>

namespace eka2l1::epoc::bt::mdns {
    // TXT keys: version = "1", room = SHA-256 of the password, address = 6-byte virtual
    // Bluetooth address. The SRV port is the Bluetooth queries port.
    constexpr const char *SERVICE_TYPE = "_eka2l1._udp";
    constexpr const char *RECORD_VERSION = "1";

    using room_digest = std::array<unsigned char, 32>;

    inline room_digest make_room_digest(const std::string &password) {
        room_digest digest{};
        mbedtls_sha256(reinterpret_cast<const unsigned char *>(password.data()), password.size(), digest.data(), 0);
        return digest;
    }

    inline std::string make_instance_name(const device_address &address) {
        char instance[32];
        std::snprintf(instance, sizeof(instance), "EKA2L1-%02x%02x%02x%02x%02x%02x",
            address.addr_[0], address.addr_[1], address.addr_[2], address.addr_[3], address.addr_[4], address.addr_[5]);
        return instance;
    }
}
