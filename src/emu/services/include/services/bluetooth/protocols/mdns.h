#pragma once

#include <cstdint>
#include <functional>
#include <services/bluetooth/protocols/common.h>
#include <memory>
#include <string>
#include <vector>
#include <services/socket/common.h>

namespace eka2l1::epoc::bt {
    struct mdns_peer {
        epoc::socket::saddress endpoint{};
        device_address address{};
    };

    // Advertises and browses _eka2l1._udp peers in the same room, over system DNS-SD on Apple
    // and raw mDNS elsewhere. All operations, including destruction, run on the libuv loop thread.
    class mdns_discovery {
        struct impl;
        std::unique_ptr<impl> impl_;

    public:
        mdns_discovery(const device_address &address, const std::string &password, std::uint16_t port, std::function<void()> changed);
        ~mdns_discovery();
        void refresh();
        std::vector<mdns_peer> peers() const;
    };
}
