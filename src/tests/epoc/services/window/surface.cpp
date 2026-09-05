/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <catch2/catch.hpp>
#include <drivers/graphics/graphics.h>
#include <services/window/surface.h>
#include <services/window/bitmap_cache.h>
#include <services/window/classes/gstore.h>
#include <services/fbs/bitmap.h>

#include <algorithm>
#include <limits>
#include <map>
#include <thread>

using namespace eka2l1;

namespace {
    class surface_driver : public drivers::graphics_driver {
        drivers::handle next_ = 1;
        drivers::handle bound_ = 0;
        bool blend_ = false;
        vec4 brush_{ 0, 0, 0, 0 };
        drivers::blend_factor factors_[4]{};

        void paint(const std::vector<std::uint8_t> &source) {
            auto &destination = images[bound_];
            if (!blend_) {
                destination = source;
                return;
            }
            destination.resize(source.size());
            for (std::size_t i = 0; i < source.size(); ++i) {
                const double alpha = source[i / 4 * 4 + 3] / 255.0;
                const auto factor = [alpha](drivers::blend_factor value) {
                    if (value == drivers::blend_factor::one) return 1.0;
                    if (value == drivers::blend_factor::frag_out_alpha) return alpha;
                    if (value == drivers::blend_factor::one_minus_frag_out_alpha) return 1.0 - alpha;
                    return 0.0;
                };
                const int offset = i % 4 == 3 ? 2 : 0;
                destination[i] = static_cast<std::uint8_t>(std::min(255.0,
                    source[i] * factor(factors_[offset]) + destination[i] * factor(factors_[offset + 1])));
            }
        }

    public:
        std::map<drivers::handle, std::vector<std::uint8_t>> images;
        std::size_t uploads = 0;
        bool invalid_use = false;

        surface_driver() : graphics_driver(drivers::graphic_api::opengl) {}
        void run() override {}
        void abort() override {}
        void update_bitmap(drivers::handle, std::size_t, const vec2 &, const vec2 &, const void *, std::size_t) override {}
        void set_viewport(const rect &) override {}
        void update_surface(void *) override {}
        void update_surface_size(const vec2 &) override {}
        void set_upscale_shader(const std::string &) override {}
        std::string get_active_upscale_shader() const override { return {}; }
        bool support_extension(drivers::graphics_driver_extension) override { return false; }
        bool query_extension_value(drivers::graphics_driver_extension_query, void *) override { return false; }

        void submit_command_list(drivers::command_list &commands) override {
            for (std::size_t i = 0; i < commands.size_; ++i) {
                auto &command = commands.base_[i];
                const auto handle = command.data_[0];
                switch (command.opcode_) {
                case drivers::graphics_driver_set_feature:
                    if (static_cast<std::uint32_t>(handle) == static_cast<std::uint32_t>(drivers::graphics_feature::blend)) {
                        blend_ = handle >> 32;
                    }
                    break;
                case drivers::graphics_driver_blend_formula:
                    factors_[0] = static_cast<drivers::blend_factor>(static_cast<std::uint32_t>(command.data_[1]));
                    factors_[1] = static_cast<drivers::blend_factor>(command.data_[1] >> 32);
                    factors_[2] = static_cast<drivers::blend_factor>(static_cast<std::uint32_t>(command.data_[2]));
                    factors_[3] = static_cast<drivers::blend_factor>(command.data_[2] >> 32);
                    break;
                case drivers::graphics_driver_set_brush_color:
                    brush_ = { static_cast<int>(handle), static_cast<int>(handle >> 32),
                        static_cast<int>(command.data_[1]), static_cast<int>(command.data_[1] >> 32) };
                    break;
                case drivers::graphics_driver_draw_rectangle:
                    paint({ static_cast<std::uint8_t>(brush_.x), static_cast<std::uint8_t>(brush_.y),
                        static_cast<std::uint8_t>(brush_.z), static_cast<std::uint8_t>(brush_.w) });
                    break;
                case drivers::graphics_driver_create_bitmap:
                case drivers::graphics_driver_create_texture: {
                    const auto created = next_++;
                    images[created] = {};
                    const auto slot = command.opcode_ == drivers::graphics_driver_create_bitmap ? 2 : 8;
                    *reinterpret_cast<drivers::handle*>(command.data_[slot]) = created;
                    break;
                }
                case drivers::graphics_driver_bind_bitmap:
                    bound_ = handle;
                    break;
                case drivers::graphics_driver_update_bitmap:
                case drivers::graphics_driver_update_texture: {
                    const auto *data = reinterpret_cast<const std::uint8_t*>(command.data_[1]);
                    invalid_use |= !images.count(handle);
                    images[handle].assign(data, data + command.data_[2]);
                    delete[] reinterpret_cast<const char*>(data);
                    ++uploads;
                    break;
                }
                case drivers::graphics_driver_draw_bitmap:
                    invalid_use |= !images.count(handle);
                    if (bound_) {
                        paint(images[handle]);
                    }
                    break;
                case drivers::graphics_driver_destroy_bitmap:
                case drivers::graphics_driver_destroy_object:
                    invalid_use |= images.erase(handle) != 1;
                    break;
                default:
                    break;
                }
                if (command.status_) {
                    *command.status_ = 0;
                }
            }
            delete[] commands.base_;
        }
    };

    void submit(surface_driver &driver, drivers::graphics_command_builder &builder) {
        auto commands = builder.retrieve_command_list();
        driver.submit_command_list(commands);
    }
}

TEST_CASE("Window background replacement is independent of producer publication", "[window_surface]") {
    auto gl = std::make_shared<epoc::window_surface>();
    auto movie = std::make_shared<epoc::window_surface>();
    epoc::window_surface_attachment window;
    const std::uint8_t white[] = { 255, 255, 255, 255 };
    const std::uint8_t red[] = { 255, 0, 0, 255 };

    window.attach(gl, {});
    window.attach(movie, {});
    REQUIRE(movie->publish_pixels(red, sizeof(red), { 1, 1 }));
    REQUIRE(gl->publish_pixels(white, sizeof(white), { 1, 1 }));
    REQUIRE(window.surface == movie);
    REQUIRE(window.changed());
    REQUIRE_FALSE(window.detach(gl));
    REQUIRE(window.surface == movie);
    REQUIRE(window.detach(movie));
    REQUIRE_FALSE(window.surface);
    REQUIRE(gl->publish_pixels(white, sizeof(white), { 1, 1 }));
    REQUIRE_FALSE(window.surface);
    window.attach(gl, {});
    REQUIRE(window.surface == gl);
}

TEST_CASE("Surface mailbox owns and coalesces complete frames", "[window_surface]") {
    epoc::window_surface surface;
    std::vector<std::uint8_t> first(16, 17);
    REQUIRE(surface.publish_pixels(first.data(), first.size(), { 2, 2 }));
    std::fill(first.begin(), first.end(), 42);
    auto pending = surface.take_pixels();
    REQUIRE(pending);
    REQUIRE(pending->rgba == std::vector<std::uint8_t>(16, 17));
    REQUIRE_FALSE(surface.take_pixels());

    REQUIRE(surface.publish_pixels(first.data(), first.size(), { 2, 2 }));
    std::fill(first.begin(), first.end(), 91);
    REQUIRE(surface.publish_pixels(first.data(), first.size(), { 2, 2 }));
    pending = surface.take_pixels();
    REQUIRE(pending->revision == 3);
    REQUIRE(pending->rgba == first);
    REQUIRE_FALSE(surface.publish_pixels(first.data(), 15, { 2, 2 }));
    REQUIRE_FALSE(surface.publish_pixels(nullptr, 16, { 2, 2 }));
    REQUIRE_FALSE(surface.publish_pixels(first.data(), 16, { -1, 2 }));
    REQUIRE_FALSE(surface.publish_pixels(first.data(), 16, { std::numeric_limits<int>::max(), std::numeric_limits<int>::max() }));
    REQUIRE(surface.revision() == 3);
}

TEST_CASE("Surface clipping preserves the mapping instead of stretching into the clip", "[window_surface]") {
    epoc::surface_configuration config;
    config.extent = rect({ 10, 20 }, { 200, 100 });
    config.clip = rect({ 60, 20 }, { 50, 100 });
    auto placement = epoc::place_surface(config, rect({ 30, 40 }, { 300, 200 }), { 640, 320 }, 2.0f, 0);
    REQUIRE(placement.destination == rect({ 80, 120 }, { 400, 200 }));
    REQUIRE(placement.source == rect({ 0, 0 }, { 640, 320 }));
    REQUIRE(placement.clip == rect({ 90, 60 }, { 50, 100 }));

    config.rotation = 90;
    placement = epoc::place_surface(config, rect({ 30, 40 }, { 300, 200 }), { 640, 320 }, 2.0f, 0);
    REQUIRE(placement.destination == rect({ 480, 120 }, { 200, 400 }));
    REQUIRE(placement.clip == rect({ 90, 60 }, { 50, 100 }));
    REQUIRE(placement.rotation == 90);

    config.clip = rect({ 500, 500 }, { 10, 10 });
    placement = epoc::place_surface(config, rect({ 30, 40 }, { 300, 200 }), { 640, 320 }, 1.0f, 0);
    REQUIRE(placement.clip.empty());
}

TEST_CASE("A presented EGL image survives further writes and producer destruction", "[window_surface]") {
    surface_driver driver;
    const auto drawing = drivers::create_bitmap(&driver, { 1, 1 }, 32);
    driver.images[drawing] = { 255, 0, 0, 255 };
    auto surface = std::make_shared<epoc::window_surface>();
    REQUIRE(surface->publish_bitmap(&driver, drawing, { 1, 1 }));
    driver.images[drawing] = { 255, 255, 255, 255 };

    epoc::window_surface_attachment window;
    window.attach(surface, {});
    surface.reset();
    drivers::graphics_command_builder builder;
    const auto presented = window.surface->prepare(&driver, builder);
    REQUIRE(driver.images[presented] == std::vector<std::uint8_t>{ 255, 0, 0, 255 });
    REQUIRE(presented != drawing);
    submit(driver, builder);
    window.surface.reset();
    REQUIRE_FALSE(driver.images.count(presented));
    REQUIRE(driver.images.count(drawing));
    REQUIRE_FALSE(driver.invalid_use);
}

TEST_CASE("Multiple targets upload one video frame and retire replaced textures in order", "[window_surface]") {
    surface_driver driver;
    auto surface = std::make_shared<epoc::window_surface>();
    const std::uint8_t red[] = { 255, 0, 0, 255 };
    REQUIRE(surface->publish_pixels(red, sizeof(red), { 1, 1 }));
    drivers::graphics_command_builder builder;
    const auto first = surface->prepare(&driver, builder);
    builder.draw_bitmap(first, 0, rect({ 0, 0 }, { 1, 1 }), {});
    REQUIRE(surface->prepare(&driver, builder) == first);
    submit(driver, builder);
    REQUIRE(driver.uploads == 1);

    const std::vector<std::uint8_t> bigger(16, 128);
    REQUIRE(surface->publish_pixels(bigger.data(), bigger.size(), { 2, 2 }));
    builder.draw_bitmap(first, 0, rect({ 0, 0 }, { 1, 1 }), {});
    const auto second = surface->prepare(&driver, builder);
    builder.draw_bitmap(second, 0, rect({ 0, 0 }, { 2, 2 }), {});
    submit(driver, builder);
    REQUIRE(first != second);
    REQUIRE_FALSE(driver.images.count(first));
    REQUIRE(driver.images[second] == bigger);
    surface.reset();
    REQUIRE(driver.images.empty());
    REQUIRE_FALSE(driver.invalid_use);
}

TEST_CASE("Publishing continues safely while window attachments are replaced and removed", "[window_surface]") {
    auto source = std::make_shared<epoc::window_surface>();
    std::atomic<bool> done{ false };
    std::thread producer([source, &done] {
        for (unsigned i = 0; i < 10000; ++i) {
            const std::vector<std::uint8_t> bytes(256, static_cast<std::uint8_t>(i));
            source->publish_pixels(bytes.data(), bytes.size(), { 8, 8 });
        }
        done = true;
    });
    std::uint64_t revision = 0;
    bool torn_frame = false;
    while (!done) {
        epoc::window_surface_attachment window;
        window.attach(source, {});
        auto replacement = std::make_shared<epoc::window_surface>();
        window.attach(replacement, {});
        window.detach(source);
        if (auto pixels = source->take_pixels()) {
            torn_frame |= pixels->revision <= revision;
            revision = pixels->revision;
            torn_frame |= !std::all_of(pixels->rgba.begin(), pixels->rgba.end(), [&](auto value) { return value == pixels->rgba.front(); });
        }
    }
    producer.join();
    REQUIRE_FALSE(torn_frame);
    REQUIRE(source->revision() == 10000);
}

TEST_CASE("Retained GDI pixels preserve alpha and redraw clears expose the surface", "[window_surface]") {
    surface_driver driver;
    epoc::bitmap_cache cache(nullptr);
    drivers::graphics_command_builder builder;
    const auto ui = drivers::create_bitmap(&driver, { 1, 1 }, 32);
    builder.bind_bitmap(ui);
    common::region clip;
    clip.add_rect(rect({ 0, 0 }, { 1, 1 }));
    epoc::gdi_command_builder gdi(&driver, builder, cache, drivers::filter_option::nearest,
        { 0, 0 }, 1.0f, clip, true);
    epoc::gdi_store_command rectangle;
    rectangle.opcode_ = epoc::gdi_store_command_draw_rect;
    auto &data = rectangle.get_data_struct<epoc::gdi_store_command_draw_rect_data>();
    data.rect_ = rect({ 0, 0 }, { 1, 1 });
    data.color_ = { 255, 0, 0, 128 };
    gdi.build_single_command(rectangle);
    submit(driver, builder);
    REQUIRE(driver.images[ui] == std::vector<std::uint8_t>{ 128, 0, 0, 128 });

    const auto screen = drivers::create_bitmap(&driver, { 1, 1 }, 32);
    driver.images[screen] = { 0, 0, 255, 255 };
    builder.bind_bitmap(screen);
    builder.blend_formula(drivers::blend_equation::add, drivers::blend_equation::add,
        drivers::blend_factor::one, drivers::blend_factor::one_minus_frag_out_alpha,
        drivers::blend_factor::one, drivers::blend_factor::one_minus_frag_out_alpha);
    builder.draw_bitmap(ui, 0, rect({ 0, 0 }, { 1, 1 }), {});
    submit(driver, builder);
    REQUIRE(driver.images[screen] == std::vector<std::uint8_t>{ 128, 0, 127, 255 });

    builder.bind_bitmap(ui);
    data.color_ = { 0, 0, 0, 0 };
    gdi.build_single_command(rectangle);
    submit(driver, builder);
    REQUIRE(driver.images[ui] == std::vector<std::uint8_t>{ 0, 0, 0, 0 });
}

TEST_CASE("Initial GDI replay consumes pending uploads without repeating pixel draws", "[window_surface]") {
    surface_driver driver;
    epoc::bitmap_cache cache(nullptr);
    drivers::graphics_command_builder builder;
    const auto ui = drivers::create_bitmap(&driver, { 1, 1 }, 32);
    const auto bitmap = drivers::create_bitmap(&driver, { 1, 1 }, 32);
    driver.images[ui] = { 0, 0, 0, 0 };
    builder.bind_bitmap(ui);
    common::region clip;
    clip.add_rect(rect({ 0, 0 }, { 1, 1 }));
    epoc::gdi_command_builder gdi(&driver, builder, cache, drivers::filter_option::nearest,
        { 0, 0 }, 1.0f, clip, true);

    epoc::gdi_store_command_segment pending;
    epoc::gdi_store_command update;
    update.opcode_ = epoc::gdi_store_command_update_texture;
    auto &upload = update.get_data_struct<epoc::gdi_store_command_update_texture_data>();
    upload = {};
    upload.handle_ = bitmap;
    upload.texture_data_ = new char[4]{ 127, 0, 0, 127 };
    upload.texture_size_ = 4;
    upload.dim_ = { 1, 1 };
    upload.pixel_per_line_ = 1;
    pending.add_command(update);

    epoc::gdi_store_command rectangle;
    rectangle.opcode_ = epoc::gdi_store_command_draw_rect;
    auto &fill = rectangle.get_data_struct<epoc::gdi_store_command_draw_rect_data>();
    fill.rect_ = rect({ 0, 0 }, { 1, 1 });
    fill.color_ = { 0, 255, 0, 255 };
    pending.add_command(rectangle);
    gdi.build_texture_updates(pending);
    submit(driver, builder);
    REQUIRE(driver.images[bitmap] == std::vector<std::uint8_t>{ 127, 0, 0, 127 });
    REQUIRE(driver.images[ui] == std::vector<std::uint8_t>{ 0, 0, 0, 0 });
    REQUIRE(driver.uploads == 1);
}

TEST_CASE("Retained GDI accepts both straight and premultiplied alpha bitmaps", "[window_surface]") {
    surface_driver driver;
    epoc::bitmap_cache cache(nullptr);
    drivers::graphics_command_builder builder;
    const auto ui = drivers::create_bitmap(&driver, { 1, 1 }, 32);
    const auto bitmap = drivers::create_bitmap(&driver, { 1, 1 }, 32);
    builder.bind_bitmap(ui);
    common::region clip;
    clip.add_rect(rect({ 0, 0 }, { 1, 1 }));
    epoc::gdi_command_builder gdi(&driver, builder, cache, drivers::filter_option::nearest,
        { 0, 0 }, 1.0f, clip, true);

    epoc::bitwise_bitmap source{};
    source.header_.size_pixels = object_size(1, 1);
    epoc::gdi_store_command draw;
    draw.opcode_ = epoc::gdi_store_command_draw_bitmap;
    auto &data = draw.get_data_struct<epoc::gdi_store_command_draw_bitmap_data>();
    data = {};
    data.main_fbs_bitmap_ = &source;
    data.main_drv_ = bitmap;
    data.gdi_flags_ = epoc::GDI_STORE_COMMAND_MAIN_RAW;
    data.dest_rect_ = rect({ 0, 0 }, { 1, 1 });
    data.source_rect_ = data.dest_rect_;

    for (const auto mode : { epoc::display_mode::color16ma, epoc::display_mode::color16map }) {
        source.settings_.current_display_mode(mode);
        driver.images[bitmap] = { static_cast<std::uint8_t>(mode == epoc::display_mode::color16ma ? 255 : 128), 0, 0, 128 };
        gdi.build_single_command(draw);
        submit(driver, builder);
        REQUIRE(driver.images[ui] == std::vector<std::uint8_t>{ 128, 0, 0, 128 });
    }
}
