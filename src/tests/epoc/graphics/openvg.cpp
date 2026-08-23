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
#include <dispatch/libraries/vg/gnuVG_path.hh>
#include <dispatch/libraries/vg/gnuVG_simplified_path.hh>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {
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

TEST_CASE("OpenVG arc fallback advances through both cubic quadrants", "[openvg]") {
    gnuVG::Context context;
    gnuVG::SimplifiedPath path(&context);
    const VGubyte segments[] = { VG_MOVE_TO, VG_SCCWARC_TO };
    // The endpoints are farther apart than the supplied unit radii, forcing
    // the half-ellipse fallback described by the OpenVG arc algorithm.
    const VGfloat coordinates[] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 3.0f, 0.0f };

    path.simplify_path(segments, coordinates, 2);

    REQUIRE(path.segments.size() == 3);
    REQUIRE(path.segments[1].t == gnuVG::SimplifiedPath::sp_cubic_to);
    REQUIRE(path.segments[1].ep != gnuVG::Point(3.0f, 0.0f));
    REQUIRE(path.segments[2].ep.x == Approx(3.0f));
    REQUIRE(path.segments[2].ep.y == Approx(0.0f));
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
