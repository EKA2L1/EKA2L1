/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <catch2/catch.hpp>
#include <dispatch/libraries/vg/gnuVG_context.hh>
#include <drivers/graphics/graphics.h>
#include <dispatch/libraries/vg/gnuVG_image.hh>
#include <dispatch/libraries/vg/register.h>
#include <dispatch/register.h>
#include <dispatch/libraries/vg/gnuVG_path.hh>
#include <dispatch/libraries/vg/gnuVG_simplified_path.hh>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

namespace {
    class mask_test_driver : public eka2l1::drivers::graphics_driver {
    public:
        std::uint64_t next_handle = 1;
        std::vector<eka2l1::vec2> texture_sizes;
        mask_test_driver() : graphics_driver(eka2l1::drivers::graphic_api::opengl) {}
        void run() override {}
        void abort() override {}
        void update_bitmap(eka2l1::drivers::handle, std::size_t, const eka2l1::vec2 &, const eka2l1::vec2 &, const void *, std::size_t) override {}
        void set_viewport(const eka2l1::rect &) override {}
        void update_surface(void *) override {}
        void update_surface_size(const eka2l1::vec2 &) override {}
        void set_upscale_shader(const std::string &) override {}
        std::string get_active_upscale_shader() const override { return {}; }
        bool support_extension(eka2l1::drivers::graphics_driver_extension) override { return false; }
        bool query_extension_value(eka2l1::drivers::graphics_driver_extension_query, void *) override { return false; }
        void submit_command_list(eka2l1::drivers::command_list &list) override {
            for (std::size_t i = 0; i < list.size_; ++i) {
                auto &command = list.base_[i];
                if (command.opcode_ == eka2l1::drivers::graphics_driver_create_texture) {
                    texture_sizes.emplace_back(static_cast<std::uint32_t>(command.data_[5]), command.data_[5] >> 32);
                    *reinterpret_cast<std::uint64_t *>(command.data_[8]) = next_handle++;
                } else if (command.opcode_ == eka2l1::drivers::graphcis_driver_create_framebuffer) {
                    *reinterpret_cast<std::uint64_t *>(command.data_[6]) = next_handle++;
                }
                if (command.status_) {
                    *command.status_ = 0;
                }
            }
            delete[] list.base_;
        }
    };

    struct stroke_mesh {
        std::vector<VGfloat> vertices;
        std::vector<unsigned int> indices;
    };

    stroke_mesh make_stroke_mesh(VGCapStyle cap_style, VGJoinStyle join_style,
        const std::vector<gnuVG::SimplifiedPath::Segment> &segments) {
        gnuVG::Context context;
        const gnuVG::GraphicState state{};
        context.vgSetf(VG_STROKE_LINE_WIDTH, 2.0f);
        context.vgSeti(state, VG_STROKE_CAP_STYLE, cap_style);
        context.vgSeti(state, VG_STROKE_JOIN_STYLE, join_style);

        gnuVG::SimplifiedPath path(&context);
        path.segments = segments;

        stroke_mesh result;
        path.get_stroke_shape([&result](const gnuVG::SimplifiedPath::StrokeData &data) {
            result.vertices.assign(data.vertices, data.vertices + data.nr_vertices * 2);
            result.indices.assign(data.indices, data.indices + data.nr_indices);
        });
        return result;
    }

    std::pair<VGfloat, VGfloat> x_extent(const stroke_mesh &mesh) {
        VGfloat minimum = mesh.vertices[0];
        VGfloat maximum = mesh.vertices[0];
        for (std::size_t i = 0; i < mesh.vertices.size(); i += 2) {
            minimum = std::min(minimum, mesh.vertices[i]);
            maximum = std::max(maximum, mesh.vertices[i]);
        }
        return { minimum, maximum };
    }
}

TEST_CASE("OpenVG path data applies scale and bias on append and modify", "[openvg]") {
    gnuVG::Context context;
    gnuVG::Path path(&context, VG_PATH_DATATYPE_S_16, 2.0f, -1.0f,
        VG_PATH_CAPABILITY_ALL);
    const VGubyte segments[] = { VG_MOVE_TO, VG_LINE_TO };
    const std::int16_t initial[] = { 1, 2, 3, 4 };

    path.vgAppendPathData(2, segments, initial);
    const std::vector<VGfloat> expected_initial = { 1.0f, 3.0f, 5.0f, 7.0f };
    REQUIRE(path.s_coordinates == expected_initial);

    const std::int16_t replacement[] = { -2, 5 };
    REQUIRE(path.vgModifyPathCoords(1, 1, replacement) == VG_NO_ERROR);
    const std::vector<VGfloat> expected_modified = { 1.0f, 3.0f, -5.0f, 9.0f };
    REQUIRE(path.s_coordinates == expected_modified);
}

TEST_CASE("OpenVG stroke cap state validates and round-trips", "[openvg]") {
    gnuVG::Context context;
    const gnuVG::GraphicState state{};

    REQUIRE(context.vgGeti(VG_STROKE_CAP_STYLE) == VG_CAP_BUTT);
    context.vgSeti(state, VG_STROKE_CAP_STYLE, VG_CAP_ROUND);
    REQUIRE(context.vgGeti(VG_STROKE_CAP_STYLE) == VG_CAP_ROUND);

    context.vgSeti(state, VG_STROKE_CAP_STYLE, 0x7fffffff);
    REQUIRE(context.get_error() == VG_ILLEGAL_ARGUMENT_ERROR);
    REQUIRE(context.vgGeti(VG_STROKE_CAP_STYLE) == VG_CAP_ROUND);
}

TEST_CASE("OpenVG oversized arc scales radii and preserves its endpoint", "[openvg]") {
    gnuVG::Context context;
    gnuVG::SimplifiedPath path(&context);
    const VGubyte segments[] = { VG_MOVE_TO, VG_SCCWARC_TO };
    // OpenVG Appendix A scales undersized radii uniformly. The resulting
    // half ellipse consists only of complete quadrants, whose transformed
    // endpoint must still be the exact endpoint supplied by the guest.
    const VGfloat coordinates[] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 3.0f, 0.0f };

    path.simplify_path(segments, coordinates, 2);

    REQUIRE(path.segments.size() == 3);
    REQUIRE(path.segments[1].t == gnuVG::SimplifiedPath::sp_cubic_to);
    REQUIRE(path.segments[1].ep != gnuVG::Point(3.0f, 0.0f));
    REQUIRE(path.segments[2].ep.x == Approx(3.0f));
    REQUIRE(path.segments[2].ep.y == 0.0f);
}

TEST_CASE("OpenVG open-contour caps extend the stroke as specified", "[openvg]") {
    using segment = gnuVG::SimplifiedPath::Segment;
    const std::vector<segment> line = {
        { gnuVG::SimplifiedPath::sp_move_to, {}, {}, { 0.0f, 0.0f } },
        { gnuVG::SimplifiedPath::sp_line_to, {}, {}, { 10.0f, 0.0f } }
    };

    const stroke_mesh butt = make_stroke_mesh(VG_CAP_BUTT, VG_JOIN_BEVEL, line);
    const stroke_mesh round = make_stroke_mesh(VG_CAP_ROUND, VG_JOIN_BEVEL, line);
    const stroke_mesh square = make_stroke_mesh(VG_CAP_SQUARE, VG_JOIN_BEVEL, line);

    REQUIRE(x_extent(butt) == std::make_pair(0.0f, 10.0f));
    REQUIRE(x_extent(round) == std::make_pair(-1.0f, 11.0f));
    REQUIRE(x_extent(square) == std::make_pair(-1.0f, 11.0f));
    REQUIRE(round.indices.size() > butt.indices.size());
}

TEST_CASE("OpenVG round and miter joins add their required outer geometry", "[openvg]") {
    using segment = gnuVG::SimplifiedPath::Segment;
    const std::vector<segment> corner = {
        { gnuVG::SimplifiedPath::sp_move_to, {}, {}, { 0.0f, 0.0f } },
        { gnuVG::SimplifiedPath::sp_line_to, {}, {}, { 10.0f, 0.0f } },
        { gnuVG::SimplifiedPath::sp_line_to, {}, {}, { 10.0f, 10.0f } }
    };

    const stroke_mesh bevel = make_stroke_mesh(VG_CAP_BUTT, VG_JOIN_BEVEL, corner);
    const stroke_mesh round = make_stroke_mesh(VG_CAP_BUTT, VG_JOIN_ROUND, corner);
    const stroke_mesh miter = make_stroke_mesh(VG_CAP_BUTT, VG_JOIN_MITER, corner);

    REQUIRE(round.indices.size() > bevel.indices.size());
    REQUIRE(miter.vertices.size() == bevel.vertices.size() + 2);
    REQUIRE(miter.indices.size() == bevel.indices.size() + 3);
}

TEST_CASE("openvg_image_limits_are_positive_and_consistent", "[openvg]") {
    gnuVG::Context context;
    const VGint width = context.vgGeti(VG_MAX_IMAGE_WIDTH);
    const VGint height = context.vgGeti(VG_MAX_IMAGE_HEIGHT);
    const VGint pixels = context.vgGeti(VG_MAX_IMAGE_PIXELS);
    const VGint bytes = context.vgGeti(VG_MAX_IMAGE_BYTES);

    REQUIRE(width >= 256);
    REQUIRE(height >= 256);
    REQUIRE(pixels >= 65536);
    REQUIRE(bytes >= 65536);
    REQUIRE(static_cast<std::int64_t>(pixels) <= static_cast<std::int64_t>(width) * height);
    REQUIRE(static_cast<std::int64_t>(bytes) >= static_cast<std::int64_t>(pixels) * 4);
    REQUIRE(context.get_error() == VG_NO_ERROR);

    const gnuVG::GraphicState state{};
    REQUIRE_THROWS_AS(gnuVG::Image(&context, state, VG_sRGBA_8888, width + 1, 1, VG_IMAGE_QUALITY_FASTER),
        gnuVG::Image::FailedToCreateImageException);
    REQUIRE(context.get_error() == VG_ILLEGAL_ARGUMENT_ERROR);
}

TEST_CASE("openvg_flush_is_patched_at_the_sdk_ordinal", "[openvg]") {
    const auto &patches = eka2l1::dispatch::LIBOPENVG11_PATCH_INFOS;
    const auto entry = std::find_if(std::begin(patches), std::end(patches), [](const auto &patch) {
        return patch.ordinal_number_ == 21;
    });
    REQUIRE(entry != std::end(patches));
    REQUIRE(std::string(eka2l1::dispatch::dispatch_funcs.at(entry->dispatch_number_).second) == "vgFlush");
}

TEST_CASE("openvg_patterns_follow_the_path_and_paint_matrices", "[openvg]") {
    struct pattern_context : gnuVG::Context {
        using gnuVG::Context::matrix_resize;
        using gnuVG::Context::image_matrix_data;
    } context;
    const gnuVG::GraphicState state{};
    context.matrix_resize(360, 640);
    context.vgSeti(state, VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
    const VGfloat path_matrix[] = { 0.05f, 0, 0, 0, -0.05f, 0, 0, 640, 1 };
    context.vgLoadMatrix(path_matrix);
    context.select_conversion_matrix(gnuVG::Context::GNUVG_MATRIX_PATH_USER_TO_SURFACE);

    gnuVG::Context::FrameBuffer image;
    image.width = 360;
    image.height = 640;
    for (const auto mode : { VG_MATRIX_FILL_PAINT_TO_USER, VG_MATRIX_STROKE_PAINT_TO_USER }) {
        context.vgSeti(state, VG_MATRIX_MODE, mode);
        const VGfloat scale = mode == VG_MATRIX_FILL_PAINT_TO_USER ? 20 : 40;
        const VGfloat paint_matrix[] = { scale, 0, 0, 0, scale, 0, 0, 0, 1 };
        context.vgLoadMatrix(paint_matrix);
        context.prepare_framebuffer_matrix(&image, nullptr, mode == VG_MATRIX_FILL_PAINT_TO_USER
            ? gnuVG::Context::GNUVG_MATRIX_FILL_PAINT_TO_USER : gnuVG::Context::GNUVG_MATRIX_STROKE_PAINT_TO_USER);
        const auto &m = context.image_matrix_data;
        REQUIRE(-m[0] + m[4] + m[12] == Approx(0));
        REQUIRE(-m[1] + m[5] + m[13] == Approx(0));
        REQUIRE(m[0] - m[4] + m[12] == Approx(20 / scale));
        REQUIRE(m[1] - m[5] + m[13] == Approx(20 / scale));
    }
}

TEST_CASE("openvg_context_starts_without_an_error_and_clears_reported_errors", "[openvg]") {
    alignas(gnuVG::Context) unsigned char storage[sizeof(gnuVG::Context)];
    std::memset(storage, 0xCD, sizeof(storage));
    auto *context = new (storage) gnuVG::Context;
    const auto initial = context->get_error();
    context->set_error(VG_ILLEGAL_ARGUMENT_ERROR);
    const auto reported = context->get_error();
    const auto cleared = context->get_error();
    context->~Context();

    REQUIRE(initial == VG_NO_ERROR);
    REQUIRE(reported == VG_ILLEGAL_ARGUMENT_ERROR);
    REQUIRE(cleared == VG_NO_ERROR);
}

TEST_CASE("OpenVG image modes default to normal and reject invalid modes", "[openvg]") {
    gnuVG::Context context;
    const gnuVG::GraphicState state{};
    REQUIRE(context.vgGeti(VG_IMAGE_MODE) == VG_DRAW_IMAGE_NORMAL);
    REQUIRE(context.vgGetVectorSize(VG_IMAGE_MODE) == 1);
    for (const VGImageMode mode : {VG_DRAW_IMAGE_STENCIL, VG_DRAW_IMAGE_MULTIPLY, VG_DRAW_IMAGE_NORMAL}) {
        context.vgSeti(state, VG_IMAGE_MODE, mode);
        REQUIRE(context.get_error() == VG_NO_ERROR);
        REQUIRE(context.vgGeti(VG_IMAGE_MODE) == mode);
        context.vgSeti(state, VG_IMAGE_MODE, 0x7fffffff);
        REQUIRE(context.get_error() == VG_ILLEGAL_ARGUMENT_ERROR);
        REQUIRE(context.vgGeti(VG_IMAGE_MODE) == mode);
    }
}

TEST_CASE("OpenVG render-to-mask validates paths and paint modes", "[openvg]") {
    gnuVG::Context context;
    const gnuVG::GraphicState state{};
    context.render_to_mask(state, VG_INVALID_HANDLE, VG_FILL_PATH, VG_SET_MASK);
    REQUIRE(context.get_error() == VG_BAD_HANDLE_ERROR);

    const auto paint = context.create<gnuVG::Paint>();
    context.render_to_mask(state, paint->get_handle(), VG_FILL_PATH, VG_SET_MASK);
    REQUIRE(context.get_error() == VG_BAD_HANDLE_ERROR);

    const auto path = context.create<gnuVG::Path>(VG_PATH_DATATYPE_F, 1.0f, 0.0f, VG_PATH_CAPABILITY_ALL);
    context.render_to_mask(state, path->get_handle(), 0x80000000, VG_SET_MASK);
    REQUIRE(context.get_error() == VG_ILLEGAL_ARGUMENT_ERROR);
    context.render_to_mask(state, path->get_handle(), VG_FILL_PATH, static_cast<VGMaskOperation>(0));
    REQUIRE(context.get_error() == VG_ILLEGAL_ARGUMENT_ERROR);
    context.render_to_mask(state, path->get_handle(), 0, VG_SET_MASK);
    REQUIRE(context.get_error() == VG_NO_ERROR);
}

TEST_CASE("OpenVG render-to-mask is patched at the SDK ordinal", "[openvg]") {
    const auto &patches = eka2l1::dispatch::LIBOPENVG11_PATCH_INFOS;
    const auto entry = std::find_if(std::begin(patches), std::end(patches), [](const auto &patch) {
        return patch.ordinal_number_ == 86;
    });
    REQUIRE(entry != std::end(patches));
    REQUIRE(std::string(eka2l1::dispatch::dispatch_funcs.at(entry->dispatch_number_).second) == "vgRenderToMask");
}

TEST_CASE("OpenVG mask storage follows the drawing surface and preserves its target", "[openvg]") {
    mask_test_driver driver;
    gnuVG::Context context;
    const gnuVG::GraphicState state{ &driver, nullptr };
    eka2l1::dispatch::egl_surface surface(nullptr, nullptr, { 360, 640 }, 99, eka2l1::dispatch::egl_config(0));
    context.draw_surface_ = &surface;
    context.on_surface_changed(&driver, nullptr, nullptr);
    auto *target = context.get_internal_framebuffer(gnuVG::Context::GNUVG_CURRENT_FRAMEBUFFER);
    REQUIRE(driver.texture_sizes.empty());
    REQUIRE(context.ensure_mask_buffers(state));
    auto *mask = context.get_internal_framebuffer(gnuVG::Context::GNUVG_MASK_BUFFER);
    REQUIRE(mask->width == 360);
    REQUIRE(mask->height == 640);
    REQUIRE(mask->color_buffer != 0);
    REQUIRE(mask->framebuffer != 0);
    REQUIRE(context.get_internal_framebuffer(gnuVG::Context::GNUVG_TEMPORARY_A)->color_buffer != 0);
    REQUIRE(context.get_internal_framebuffer(gnuVG::Context::GNUVG_CURRENT_FRAMEBUFFER) == target);
    const auto allocations = driver.texture_sizes.size();
    REQUIRE(context.ensure_mask_buffers(state));
    REQUIRE(driver.texture_sizes.size() == allocations);

    surface.dimension_ = { 640, 360 };
    context.on_draw_surface_resized(&driver);
    REQUIRE(context.ensure_mask_buffers(state));
    REQUIRE(mask->width == 640);
    REQUIRE(mask->height == 360);
    REQUIRE(context.get_internal_framebuffer(gnuVG::Context::GNUVG_CURRENT_FRAMEBUFFER) == target);
    REQUIRE(context.vgGeti(VG_MASKING) == VG_FALSE);
    const auto commands = context.cmd_builder_.retrieve_command_list();
    delete[] commands.base_;
}
