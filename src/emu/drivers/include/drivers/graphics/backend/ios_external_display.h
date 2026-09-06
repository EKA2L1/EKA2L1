// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <common/vecx.h>
#include <memory>

namespace eka2l1::drivers {
    class ios_external_display {
    public:
        ios_external_display();
        ~ios_external_display();
        void set_surface(void *layer, bool enabled);
        void enqueue_frame(const rect &crop, const vec2 &surface_size);
        // Called on the graphics thread before the phone drawable is presented.
        void present(void *context, unsigned int source_framebuffer);
        void release();

    private:
        struct impl;
        std::unique_ptr<impl> impl_;
    };
}
