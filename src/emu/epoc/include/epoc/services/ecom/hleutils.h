/*
 * Copyright (c) 2020 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

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