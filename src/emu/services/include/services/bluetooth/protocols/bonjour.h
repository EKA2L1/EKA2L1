#pragma once

#include <cstdint>
#include <functional>
#include <services/bluetooth/protocols/common.h>
#include <memory>
#include <string>
#include <vector>
#include <services/socket/common.h>

namespace eka2l1::epoc::bt {
    struct bonjour_peer {
        epoc::socket::saddress endpoint{};
        device_address address{};
    };

    // All operations, including destruction, run on the libuv loop thread.
    class bonjour_discovery {
        struct impl;
        std::unique_ptr<impl> impl_;

    public:
        bonjour_discovery(const device_address &address, const std::string &password, std::uint16_t port, std::function<void()> changed);
        ~bonjour_discovery();
        void retry();
        std::vector<bonjour_peer> peers() const;
    };
}
