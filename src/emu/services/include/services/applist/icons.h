// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <common/types.h>
#include <common/vecx.h>
#include <optional>
#include <vector>

namespace eka2l1 {
    class fbs_server;
    namespace common {
        class ro_stream;
    }

    struct app_icon_image {
        vec2 size;
        // Premultiplied RGBA8888 with tightly packed rows, owned independently of guest memory.
        std::vector<std::uint8_t> pixels;
    };

    std::optional<app_icon_image> decode_mbm_app_icon(fbs_server *fbs, common::ro_stream &stream, epocver version);
}
