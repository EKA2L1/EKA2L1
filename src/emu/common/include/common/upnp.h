// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>

namespace UPnP
{
void TryPortmapping(std::uint16_t port, bool is_udp);
void StopPortmapping(std::uint16_t port, bool is_udp);
}  // namespace UPnP
