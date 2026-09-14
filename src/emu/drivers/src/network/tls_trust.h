// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1::drivers {
    using tls_certificate_chain = std::vector<std::vector<std::uint8_t>>;
    bool verify_system_certificate(const tls_certificate_chain &chain, const std::string &hostname);
}
