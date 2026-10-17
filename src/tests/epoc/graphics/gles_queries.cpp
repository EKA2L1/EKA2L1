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
#include <array>
#include <limits>
#include <dispatch/libraries/gles1/def.h>
#include <dispatch/libraries/gles2/def.h>

using namespace eka2l1::dispatch;

TEST_CASE("GLES boolean queries write one canonical byte per component", "[gles1][gles2]") {
    egl_context_es1 es1;
    egl_context_es2 es2;
    for (egl_context_es_shared *context : { static_cast<egl_context_es_shared*>(&es1),
             static_cast<egl_context_es_shared*>(&es2) }) {
        std::array<std::uint8_t, 18> result;
        const auto query = [&](std::uint32_t pname) {
            result.fill(0xA5);
            REQUIRE(context->get_data(nullptr, pname, result.data() + 1, GLES_GET_DATA_TYPE_BOOLEAN));
            REQUIRE(result.front() == 0xA5);
        };
        query(GL_COLOR_WRITEMASK_EMU);
        REQUIRE((std::array<std::uint8_t, 4>{result[1], result[2], result[3], result[4]} ==
            std::array<std::uint8_t, 4>{1, 1, 1, 1}));
        REQUIRE(result[5] == 0xA5);

        gl_fixed fixed_mask[4]{};
        REQUIRE(context->get_data(nullptr, GL_COLOR_WRITEMASK_EMU, fixed_mask, GLES_GET_DATA_TYPE_FIXED));
        for (const auto component : fixed_mask) {
            REQUIRE(component == 65536);
        }

        context->color_mask_ = 0b0101;
        query(GL_COLOR_WRITEMASK_EMU);
        REQUIRE((std::array<std::uint8_t, 4>{result[1], result[2], result[3], result[4]} ==
            std::array<std::uint8_t, 4>{1, 0, 1, 0}));
        REQUIRE(result[5] == 0xA5);

        context->clear_color_[0] = 0.0f;
        context->clear_color_[1] = 0.001f;
        context->clear_color_[2] = -0.5f;
        context->clear_color_[3] = 1.0f;
        query(GL_COLOR_CLEAR_VALUE_EMU);
        REQUIRE((std::array<std::uint8_t, 4>{result[1], result[2], result[3], result[4]} ==
            std::array<std::uint8_t, 4>{0, 1, 1, 1}));
        REQUIRE(result[5] == 0xA5);

        query(context == &es1 ? GL_POINT_SIZE_ARRAY_TYPE_OES : GL_MAX_VERTEX_ATTRIBS_EMU);
        REQUIRE(result[1] == 1);
        REQUIRE(result[2] == 0xA5);
    }

    es1.point_distance_attenuation_[0] = 0.0f;
    es1.point_distance_attenuation_[1] = 0.001f;
    es1.point_distance_attenuation_[2] = -0.5f;
    std::array<std::uint8_t, 14> result;
    result.fill(0xA5);
    REQUIRE(es1.get_data(nullptr, GL_POINT_DISTANCE_ATTENUATION_EMU, result.data() + 1, GLES_GET_DATA_TYPE_BOOLEAN));
    REQUIRE(result[0] == 0xA5);
    REQUIRE(result[1] == 0);
    REQUIRE(result[2] == 1);
    REQUIRE(result[3] == 1);
    REQUIRE(result[4] == 0xA5);

    std::array<std::uint8_t, GLES_EMU_COMPRESSED_TEX_FORMAT_COUNT * 4 + 2> formats;
    formats.fill(0xA5);
    REQUIRE(es1.get_data(nullptr, GL_COMPRESSED_TEXTURE_FORMATS_EMU, formats.data() + 1, GLES_GET_DATA_TYPE_BOOLEAN));
    REQUIRE(formats[0] == 0xA5);
    for (std::size_t i = 1; i <= GLES_EMU_COMPRESSED_TEX_FORMAT_COUNT; ++i) {
        REQUIRE(formats[i] == 1);
    }
    REQUIRE(formats[GLES_EMU_COMPRESSED_TEX_FORMAT_COUNT + 1] == 0xA5);
    gl_fixed fixed_count = 0;
    REQUIRE(es1.get_data(nullptr, GL_NUM_COMPRESSED_TEXTURE_FORMATS_EMU, &fixed_count, GLES_GET_DATA_TYPE_FIXED));
    REQUIRE(fixed_count == GLES_EMU_COMPRESSED_TEX_FORMAT_COUNT * 65536);
}

TEST_CASE("GLES1 enable state can be saved and restored through Get queries", "[gles1]") {
    egl_context_es1 context;
    for (const auto feature : { GL_LIGHTING_EMU, GL_LIGHT0_EMU, GL_FOG_EMU,
             GL_COLOR_MATERIAL_EMU, GL_NORMALIZE_EMU, GL_RESCALE_NORMAL_EMU,
             GL_BLEND_EMU, GL_DEPTH_TEST_EMU, GL_SCISSOR_TEST_EMU }) {
        std::array<std::uint8_t, 6> result;
        result.fill(0xA5);
        REQUIRE(context.get_data(nullptr, feature, result.data() + 1, GLES_GET_DATA_TYPE_BOOLEAN));
        REQUIRE(result[1] == 0);
        REQUIRE(result[0] == 0xA5);
        REQUIRE(result[2] == 0xA5);
        REQUIRE(context.enable(feature));
        REQUIRE(context.get_data(nullptr, feature, result.data() + 1, GLES_GET_DATA_TYPE_BOOLEAN));
        REQUIRE(result[1] == 1);
        REQUIRE(result[2] == 0xA5);
        float floating = 0.0f;
        REQUIRE(context.get_data(nullptr, feature, &floating, GLES_GET_DATA_TYPE_FLOAT));
        REQUIRE(floating == 1.0f);
        std::int32_t integer = 0;
        REQUIRE(context.get_data(nullptr, feature, &integer, GLES_GET_DATA_TYPE_INTEGER));
        REQUIRE(integer == 1);
        gl_fixed fixed = 0;
        REQUIRE(context.get_data(nullptr, feature, &fixed, GLES_GET_DATA_TYPE_FIXED));
        REQUIRE(fixed == 65536);
        REQUIRE(context.disable(feature));
        REQUIRE(context.get_data(nullptr, feature, &integer, GLES_GET_DATA_TYPE_INTEGER));
        REQUIRE(integer == 0);
    }
}

TEST_CASE("GLES rendering state queries preserve depth and raster state", "[gles1][gles2]") {
    egl_context_es1 es1;
    egl_context_es2 es2;
    for (egl_context_es_shared *context : { static_cast<egl_context_es_shared*>(&es1),
             static_cast<egl_context_es_shared*>(&es2) }) {
        std::uint8_t boolean = 0;
        REQUIRE(context->get_data(nullptr, GL_DEPTH_WRITEMASK_EMU, &boolean, GLES_GET_DATA_TYPE_BOOLEAN));
        REQUIRE(boolean == 1);
        gl_fixed fixed_boolean = 0;
        REQUIRE(context->get_data(nullptr, GL_DEPTH_WRITEMASK_EMU, &fixed_boolean, GLES_GET_DATA_TYPE_FIXED));
        REQUIRE(fixed_boolean == 65536);
        context->depth_mask_ = 0;
        REQUIRE(context->get_data(nullptr, GL_DEPTH_WRITEMASK_EMU, &boolean, GLES_GET_DATA_TYPE_BOOLEAN));
        REQUIRE(boolean == 0);

        std::int32_t integer = 0;
        context->depth_func_ = GL_LEQUAL_EMU;
        REQUIRE(context->get_data(nullptr, GL_DEPTH_FUNC_EMU, &integer, GLES_GET_DATA_TYPE_INTEGER));
        REQUIRE(integer == GL_LEQUAL_EMU);
        gl_fixed fixed_enum = 0;
        REQUIRE(context->get_data(nullptr, GL_DEPTH_FUNC_EMU, &fixed_enum, GLES_GET_DATA_TYPE_FIXED));
        REQUIRE(fixed_enum == GL_LEQUAL_EMU);
        context->active_front_face_rule_ = eka2l1::drivers::rendering_face_determine_rule::vertices_clockwise;
        REQUIRE(context->get_data(nullptr, GL_FRONT_FACE_EMU, &integer, GLES_GET_DATA_TYPE_INTEGER));
        REQUIRE(integer == GL_CLOCKWISE_EMU);

        context->scissor_bl_ = eka2l1::rect(eka2l1::vec2(-3, 7), eka2l1::vec2(200, 100));
        std::int32_t box[4]{};
        REQUIRE(context->get_data(nullptr, GL_SCISSOR_BOX_EMU, box, GLES_GET_DATA_TYPE_INTEGER));
        REQUIRE(box[0] == -3);
        REQUIRE(box[1] == 7);
        REQUIRE(box[2] == 200);
        REQUIRE(box[3] == 100);
        gl_fixed fixed_box[4]{};
        REQUIRE(context->get_data(nullptr, GL_SCISSOR_BOX_EMU, fixed_box, GLES_GET_DATA_TYPE_FIXED));
        REQUIRE(fixed_box[0] == -3 * 65536);
        REQUIRE(fixed_box[1] == 7 * 65536);
        REQUIRE(fixed_box[2] == 200 * 65536);
        REQUIRE(fixed_box[3] == 100 * 65536);

        context->depth_range_min_ = 0.25f;
        context->depth_range_max_ = 0.75f;
        float range[2]{};
        REQUIRE(context->get_data(nullptr, GL_DEPTH_RANGE_EMU, range, GLES_GET_DATA_TYPE_FLOAT));
        REQUIRE(range[0] == 0.25f);
        REQUIRE(range[1] == 0.75f);
        gl_fixed fixed_range[2]{};
        REQUIRE(context->get_data(nullptr, GL_DEPTH_RANGE_EMU, fixed_range, GLES_GET_DATA_TYPE_FIXED));
        REQUIRE(fixed_range[0] == 16384);
        REQUIRE(fixed_range[1] == 49152);
        context->depth_range_min_ = 0.0f;
        context->depth_range_max_ = 1.0f;
        std::int32_t integer_range[2]{};
        REQUIRE(context->get_data(nullptr, GL_DEPTH_RANGE_EMU, integer_range, GLES_GET_DATA_TYPE_INTEGER));
        REQUIRE(integer_range[0] == 0);
        REQUIRE(integer_range[1] == std::numeric_limits<std::int32_t>::max());

        context->polygon_offset_factor_ = -2.5f;
        context->polygon_offset_units_ = 1.25f;
        float floating = 0.0f;
        REQUIRE(context->get_data(nullptr, GL_POLYGON_OFFSET_FACTOR_EMU, &floating, GLES_GET_DATA_TYPE_FLOAT));
        REQUIRE(floating == -2.5f);
        REQUIRE(context->get_data(nullptr, GL_POLYGON_OFFSET_FACTOR_EMU, &integer, GLES_GET_DATA_TYPE_INTEGER));
        REQUIRE(integer == -3);
        REQUIRE(context->get_data(nullptr, GL_POLYGON_OFFSET_UNITS_EMU, &floating, GLES_GET_DATA_TYPE_FLOAT));
        REQUIRE(floating == 1.25f);
        context->polygon_offset_factor_ = std::numeric_limits<float>::max();
        gl_fixed fixed_offset = 0;
        REQUIRE(context->get_data(nullptr, GL_POLYGON_OFFSET_FACTOR_EMU, &fixed_offset, GLES_GET_DATA_TYPE_FIXED));
        REQUIRE(fixed_offset == std::numeric_limits<std::int32_t>::max());
        context->polygon_offset_factor_ = -std::numeric_limits<float>::max();
        REQUIRE(context->get_data(nullptr, GL_POLYGON_OFFSET_FACTOR_EMU, &fixed_offset, GLES_GET_DATA_TYPE_FIXED));
        REQUIRE(fixed_offset == std::numeric_limits<std::int32_t>::min());
    }
    std::int32_t model = 0;
    REQUIRE(es1.get_data(nullptr, GL_SHADE_MODEL_EMU, &model, GLES_GET_DATA_TYPE_INTEGER));
    REQUIRE(model == GL_SMOOTH_EMU);
    es1.fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_SHADE_MODEL_FLAT;
    REQUIRE(es1.get_data(nullptr, GL_SHADE_MODEL_EMU, &model, GLES_GET_DATA_TYPE_INTEGER));
    REQUIRE(model == GL_FLAT_EMU);
}

TEST_CASE("GLES scissor query resolves the initial surface dimensions before drawing", "[gles1][gles2]") {
    egl_config config(0);
    egl_surface surface(nullptr, nullptr, {640, 360}, 7, config);
    egl_context_es1 es1;
    egl_context_es2 es2;
    for (egl_context_es_shared *context : { static_cast<egl_context_es_shared*>(&es1),
             static_cast<egl_context_es_shared*>(&es2) }) {
        context->draw_surface_ = &surface;
        context->on_surface_changed(nullptr, nullptr, nullptr);
        std::int32_t box[4]{};
        REQUIRE(context->get_data(nullptr, GL_SCISSOR_BOX_EMU, box, GLES_GET_DATA_TYPE_INTEGER));
        REQUIRE(box[0] == 0);
        REQUIRE(box[1] == 0);
        REQUIRE(box[2] == 640);
        REQUIRE(box[3] == 360);
        context->draw_surface_ = nullptr;
    }
}
