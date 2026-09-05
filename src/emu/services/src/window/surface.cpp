/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <services/window/surface.h>

#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>

#include <limits>

namespace eka2l1::epoc {
    window_surface::~window_surface() {
        if (handle_) {
            drivers::graphics_command_builder builder;
            if (bitmap_) {
                builder.destroy_bitmap(handle_);
            } else {
                builder.destroy(handle_);
            }
            auto commands = builder.retrieve_command_list();
            driver_->submit_command_list(commands);
        }
    }

    bool window_surface::publish_pixels(const std::uint8_t *rgba, std::size_t length, const eka2l1::vec2 &size) {
        if (!rgba || size.x <= 0 || size.y <= 0
            || static_cast<std::size_t>(size.x) > std::numeric_limits<std::size_t>::max() / 4 / size.y) {
            return false;
        }
        const std::size_t required = static_cast<std::size_t>(size.x) * size.y * 4;
        if (length < required) {
            return false;
        }

        surface_pixels pixels{ size, std::vector<std::uint8_t>(rgba, rgba + required), 0 };
        const std::lock_guard<std::mutex> guard(pixels_lock_);
        pixels.revision = revision_ + 1;
        pending_pixels_ = std::move(pixels);
        revision_++;
        return true;
    }

    std::optional<surface_pixels> window_surface::take_pixels() {
        const std::lock_guard<std::mutex> guard(pixels_lock_);
        auto pixels = std::move(pending_pixels_);
        pending_pixels_.reset();
        return pixels;
    }

    bool window_surface::publish_bitmap(drivers::graphics_driver *driver, drivers::handle source, const eka2l1::vec2 &size) {
        if (!source || size.x <= 0 || size.y <= 0) {
            return false;
        }

        driver_ = driver;
        drivers::graphics_command_builder builder;
        if (!handle_) {
            handle_ = drivers::create_bitmap(driver, size, 32);
            if (!handle_) {
                return false;
            }
            bitmap_ = true;
        } else if (size != size_) {
            builder.resize_bitmap(handle_, size);
        }

        size_ = size;
        builder.bind_bitmap(handle_);
        builder.set_feature(drivers::graphics_feature::clipping, false);
        builder.set_feature(drivers::graphics_feature::stencil_test, false);
        builder.set_feature(drivers::graphics_feature::depth_test, false);
        builder.set_feature(drivers::graphics_feature::blend, false);
        builder.set_feature(drivers::graphics_feature::cull, false);
        builder.set_color_mask(0xF);
        builder.draw_bitmap(source, 0, eka2l1::rect({ 0, 0 }, size), eka2l1::rect({ 0, 0 }, size),
            { 0, 0 }, 0.0f, drivers::bitmap_draw_flag_flip);
        builder.bind_bitmap(source);

        auto commands = builder.retrieve_command_list();
        driver->submit_command_list(commands);
        prepared_revision_ = ++revision_;
        return true;
    }

    drivers::handle window_surface::prepare(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder) {
        auto pixels = take_pixels();
        if (!pixels) {
            return handle_;
        }

        driver_ = driver;
        const eka2l1::vec3 size(pixels->size.x, pixels->size.y, 0);
        if (!handle_ || pixels->size != size_) {
            if (handle_) {
                builder.destroy(handle_);
            }
            handle_ = drivers::create_texture(driver, 2, 0, drivers::texture_format::rgba,
                drivers::texture_format::rgba, drivers::texture_data_type::ubyte, nullptr, 0, size);
            size_ = pixels->size;
        }

        if (handle_) {
            builder.update_texture(handle_, reinterpret_cast<const char*>(pixels->rgba.data()), pixels->rgba.size(), 0,
                drivers::texture_format::rgba, drivers::texture_data_type::ubyte, { 0, 0, 0 }, size);
            prepared_revision_ = pixels->revision;
        }
        return handle_;
    }

    surface_placement place_surface(const surface_configuration &config, const eka2l1::rect &window,
        const eka2l1::vec2 &pixels, float scale, int native_rotation) {
        const eka2l1::rect bounds({ 0, 0 }, window.size);
        const eka2l1::rect extent = config.extent.value_or(bounds);
        surface_placement result;
        result.clip = config.clip.value_or(bounds).intersect(bounds).intersect(extent);
        result.clip.top += window.top;
        const eka2l1::rect source_bounds({ 0, 0 }, pixels);
        result.source = config.viewport.value_or(source_bounds).intersect(source_bounds);
        result.destination = extent;
        result.destination.top += window.top;
        result.destination.scale(scale);
        result.rotation = config.native_orientation ? native_rotation : config.rotation;
        drivers::advance_draw_pos_around_origin(result.destination, result.rotation);
        if ((result.rotation % 180) != 0) {
            std::swap(result.destination.size.x, result.destination.size.y);
        }
        return result;
    }

    void window_surface_attachment::attach(const std::shared_ptr<window_surface> &source, const surface_configuration &configuration) {
        surface = source;
        config = configuration;
        last_revision = 0;
    }

    bool window_surface_attachment::detach(const std::shared_ptr<window_surface> &source) {
        if (!source || surface != source) {
            return false;
        }
        surface.reset();
        last_revision = 0;
        return true;
    }

    bool window_surface_attachment::changed() const {
        return surface && surface->revision() != last_revision;
    }
}
