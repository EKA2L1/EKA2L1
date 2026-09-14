// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <dispatch/def.h>
#include <drivers/network/tls.h>
#include <utils/des.h>

#include <unordered_map>

namespace eka2l1::dispatch {
    class tls_controller {
        struct entry {
            std::uint64_t owner;
            std::unique_ptr<drivers::tls_session> session;
        };
        std::uint32_t next_handle_ = 1;
        std::unordered_map<std::uint32_t, entry> sessions_;

    public:
        std::int32_t create(std::uint64_t owner, const std::string &hostname, const std::string &peer_address);
        drivers::tls_session *get(std::uint64_t owner, std::uint32_t handle);
        bool erase(std::uint64_t owner, std::uint32_t handle);
        void erase_process(std::uint64_t owner);
    };

    BRIDGE_FUNC_DISPATCHER(std::int32_t, etls_create, epoc::desc8 *hostname, epoc::desc8 *peer_address);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, etls_destroy, std::uint32_t handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, etls_command, std::uint32_t handle, std::uint32_t operation,
        epoc::desc8 *input, epoc::des8 *output);
}
