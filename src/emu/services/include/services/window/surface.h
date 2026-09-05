/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <common/vecx.h>
#include <drivers/graphics/common.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace eka2l1::drivers {
    class graphics_driver;
    class graphics_command_builder;
}

namespace eka2l1::epoc {
    struct surface_pixels {
        eka2l1::vec2 size;
        std::vector<std::uint8_t> rgba;
        std::uint64_t revision;
    };

    // Async producers publish bytes only. GPU work and final release are
    // serialized with WServ composition by the kernel lock.
    class window_surface {
        std::mutex pixels_lock_;
        std::optional<surface_pixels> pending_pixels_;
        std::atomic<std::uint64_t> revision_{ 0 };
        std::atomic<bool> streaming_{ false };

        drivers::graphics_driver *driver_ = nullptr;
        drivers::handle handle_ = 0;
        eka2l1::vec2 size_{ 0, 0 };
        bool bitmap_ = false;
        std::uint64_t prepared_revision_ = 0;

    public:
        ~window_surface();

        bool publish_pixels(const std::uint8_t *rgba, std::size_t length, const eka2l1::vec2 &size);
        std::optional<surface_pixels> take_pixels();
        bool publish_bitmap(drivers::graphics_driver *driver, drivers::handle source, const eka2l1::vec2 &size);
        drivers::handle prepare(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder);

        std::uint64_t revision() const { return revision_.load(); }
        std::uint64_t prepared_revision() const { return prepared_revision_; }
        bool streaming() const { return streaming_.load(); }
        void set_streaming(bool streaming) { streaming_ = streaming; }
        const eka2l1::vec2 &size() const { return size_; }
    };

    struct surface_configuration {
        std::optional<eka2l1::rect> extent;
        std::optional<eka2l1::rect> clip;
        std::optional<eka2l1::rect> viewport;
        int rotation = 0;
        bool native_orientation = false;
    };

    struct surface_placement {
        eka2l1::rect destination;
        eka2l1::rect source;
        eka2l1::rect clip;
        int rotation;
    };

    surface_placement place_surface(const surface_configuration &config, const eka2l1::rect &window,
        const eka2l1::vec2 &pixels, float scale, int native_rotation);

    struct window_surface_attachment {
        std::shared_ptr<window_surface> surface;
        surface_configuration config;
        std::uint64_t last_revision = 0;

        void attach(const std::shared_ptr<window_surface> &source, const surface_configuration &configuration);
        bool detach(const std::shared_ptr<window_surface> &source);
        bool changed() const;
    };
}
