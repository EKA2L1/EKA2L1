#pragma once

#include <epoc/services/faker.h>
#include <epoc/utils/uid.h>
#include <epoc/ptr.h>

#include <cstdint>

namespace eka2l1 {
    class ecom_server;
}

namespace eka2l1::epoc {
    struct implementation_proxy {
        epoc::uid         implementation_uid;
        eka2l1::ptr<void> factory_method;
    };

    struct implementation_proxy_3 {
        epoc::uid         implementation_uid;
        eka2l1::ptr<void> factory_method;
        eka2l1::ptr<void> interface_get_method;
        eka2l1::ptr<void> interface_release_method;
        std::int32_t      reserved_for_some_reason[4];
    };

    service::faker::chain *get_implementation_proxy_table(service::faker *pr, eka2l1::ecom_server *serv,
        const std::uint32_t impl_uid);
}