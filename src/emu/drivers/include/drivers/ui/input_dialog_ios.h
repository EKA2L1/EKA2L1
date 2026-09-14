// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <cstdint>

namespace eka2l1::drivers::ui {
    void set_automatic_input_view(bool automatic);
    void present_input_view();
    void request_input_view(std::function<void()> activate_editor);
    void set_input_available(std::uint64_t owner, bool available);
    bool is_input_available();
    void reset_input_view();
}
