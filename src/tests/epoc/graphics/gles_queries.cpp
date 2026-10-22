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
#include <dispatch/libraries/egl/egl.h>
#include <services/window/bitmap_cache.h>

using namespace eka2l1::dispatch;

TEST_CASE("GLES2 rejects GLES1 logic operation capabilities without changing blending", "[gles2][logicop]") {
    egl_context_es2 context;
    REQUIRE(context.enable(GL_BLEND_EMU));
    REQUIRE_FALSE(context.enable(GL_COLOR_LOGIC_OP_EMU));
    REQUIRE_FALSE(context.disable(GL_COLOR_LOGIC_OP_EMU));
    bool enabled = false;
    REQUIRE_FALSE(context.is_enabled(GL_COLOR_LOGIC_OP_EMU, enabled));
    REQUIRE(context.is_enabled(GL_BLEND_EMU, enabled));
    REQUIRE(enabled);
    auto commands = context.cmd_builder_.retrieve_command_list();
    REQUIRE(commands.size_ == 1);
    eka2l1::drivers::graphics_feature feature;
    bool host_enabled = false;
    eka2l1::drivers::unpack_u64_to_2u32(commands.base_[0].data_[0], feature, host_enabled);
    REQUIRE(feature == eka2l1::drivers::graphics_feature::blend);
    REQUIRE(host_enabled);
    delete[] commands.base_;
}

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

namespace {
    template <typename Context>
    struct buffer_test_context : Context {
        ~buffer_test_context() {
            this->buffers_.clear();
            this->objects_.clear();
        }
    };
}

// OpenGL ES 1.1/2.0 sections 2.9 and 6.1.6 distinguish reserved names from objects.
TEST_CASE("GLES buffer names become objects only on their first binding", "[gles1][gles2]") {
    buffer_test_context<egl_context_es1> es1;
    buffer_test_context<egl_context_es2> es2;
    for (egl_context_es_shared *context : { static_cast<egl_context_es_shared*>(&es1),
             static_cast<egl_context_es_shared*>(&es2) }) {
        gles_driver_object_instance texture = std::make_unique<gles_driver_texture>(*context);
        const auto texture_name = static_cast<std::uint32_t>(context->objects_.add(texture));
        auto *texture_object = context->objects_.get(texture_name)->get();
        context->delete_buffer(texture_name);
        REQUIRE(context->objects_.get(texture_name)->get() == texture_object);
        REQUIRE_FALSE(context->is_buffer(0));
        REQUIRE_FALSE(context->is_buffer(texture_name));

        const auto first = context->generate_buffer();
        const auto second = context->generate_buffer();
        REQUIRE(first != 0);
        REQUIRE(second != 0);
        REQUIRE(first != second);
        REQUIRE_FALSE(context->is_buffer(first));
        REQUIRE_FALSE(context->is_buffer(second));
        REQUIRE(context->bind_buffer(GL_ELEMENT_ARRAY_BUFFER_EMU, first) == 0);
        REQUIRE(context->is_buffer(first));
        auto *buffer = context->binded_buffer(false);
        buffer->assign_data_size(128);
        REQUIRE(context->bind_buffer(GL_ARRAY_BUFFER_EMU, first) == 0);
        REQUIRE(context->binded_buffer(true) == buffer);
        REQUIRE(buffer->data_size() == 128);
        REQUIRE(context->bind_buffer(GL_ARRAY_BUFFER_EMU, 0) == 0);
        REQUIRE(context->is_buffer(first));
        REQUIRE(context->binded_buffer(false) == buffer);

        context->attrib_changed_ = false;
        context->delete_buffer(0);
        context->delete_buffer(0xFFFFFFFF);
        REQUIRE_FALSE(context->attrib_changed_);
        REQUIRE(context->binded_buffer(false) == buffer);
        context->delete_buffer(first);
        REQUIRE_FALSE(context->is_buffer(first));
        REQUIRE(context->binded_element_array_buffer_handle_ == 0);
        REQUIRE_FALSE(context->is_buffer(second));
        REQUIRE(context->objects_.get(texture_name)->get() == texture_object);
        REQUIRE(context->bind_buffer(GL_ARRAY_BUFFER_EMU, first) == 0);
        REQUIRE(context->binded_buffer(true)->data_size() == 0);
        REQUIRE(context->is_buffer(first));
        context->delete_buffer(second);
        const auto replacement = context->generate_buffer();
        REQUIRE(replacement != 0);
        REQUIRE(replacement != first);
        REQUIRE_FALSE(context->is_buffer(replacement));
    }
}

// ES 1.1/2.0 2.8 and 2.9.2: indices from an element buffer select vertices up to their maximum, not count.
TEST_CASE("GLES element buffers report the highest index of the drawn range", "[gles1][gles2]") {
    buffer_test_context<egl_context_es1> es1;
    buffer_test_context<egl_context_es2> es2;
    for (egl_context_es_shared *context : { static_cast<egl_context_es_shared*>(&es1),
             static_cast<egl_context_es_shared*>(&es2) }) {
        const std::array<std::uint16_t, 4> indices = { 2, 688, 5, 1 };
        gles_driver_buffer buffer(*context);
        buffer.assign_data_size(sizeof(indices));
        buffer.assign_index_data(GL_ELEMENT_ARRAY_BUFFER_EMU, indices.data(), sizeof(indices));
        REQUIRE(buffer.max_index(0, 4, GL_UNSIGNED_SHORT_EMU) == 688);
        REQUIRE(buffer.max_index(4, 2, GL_UNSIGNED_SHORT_EMU) == 5);
        REQUIRE(buffer.max_index(1, 3, GL_UNSIGNED_BYTE_EMU) == 176);
        REQUIRE(buffer.max_index(2, 4, GL_UNSIGNED_SHORT_EMU) == -1);
        REQUIRE(buffer.max_index(0, 0, GL_UNSIGNED_SHORT_EMU) == -1);

        const std::uint16_t replacement = 900;
        buffer.update_index_data(6, &replacement, sizeof(replacement));
        REQUIRE(buffer.max_index(4, 2, GL_UNSIGNED_SHORT_EMU) == 900);
        buffer.update_index_data(7, &replacement, sizeof(replacement));
        REQUIRE(buffer.max_index(4, 2, GL_UNSIGNED_SHORT_EMU) == 900);

        buffer.assign_data_size(16);
        buffer.assign_index_data(GL_ELEMENT_ARRAY_BUFFER_EMU, nullptr, 16);
        REQUIRE(buffer.max_index(0, 8, GL_UNSIGNED_SHORT_EMU) == 0);

        buffer.assign_index_data(GL_ARRAY_BUFFER_EMU, indices.data(), sizeof(indices));
        REQUIRE(buffer.max_index(0, 4, GL_UNSIGNED_SHORT_EMU) == -1);
    }
}

TEST_CASE("GLES1 deleting a buffer detaches every captured array binding", "[gles1]") {
    buffer_test_context<egl_context_es1> context;
    REQUIRE(context.bind_buffer(GL_ARRAY_BUFFER_EMU, 7) == 0);
    context.binded_buffer(true)->assign_handle(42);
    REQUIRE(context.bind_buffer(GL_ELEMENT_ARRAY_BUFFER_EMU, 7) == 0);
    gles_vertex_attrib *attributes[] = { &context.vertex_attrib_, &context.color_attrib_, &context.normal_attrib_,
        &context.matrix_index_attrib_, &context.weight_attrib_, &context.point_size_attrib_ };
    for (auto *attribute : attributes) {
        attribute->buffer_obj_ = 7;
        attribute->offset_ = 12;
    }
    for (auto &unit : context.texture_units_) {
        unit.coord_attrib_.buffer_obj_ = 7;
    }
    context.texture_units_[1].coord_attrib_.buffer_obj_ = 8;
    REQUIRE(context.bind_buffer(GL_ARRAY_BUFFER_EMU, 8) == 0);
    context.attrib_changed_ = false;
    context.delete_buffer(7);
    REQUIRE(context.binded_array_buffer_handle_ == 8);
    REQUIRE(context.binded_element_array_buffer_handle_ == 0);
    REQUIRE(context.attrib_changed_);
    REQUIRE_FALSE(context.is_buffer(7));
    REQUIRE(context.buffer_pools_.top() == 42);
    for (const auto *attribute : attributes) {
        REQUIRE(attribute->buffer_obj_ == 0);
        REQUIRE(attribute->offset_ == 12);
    }
    for (std::size_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; ++i) {
        REQUIRE(context.texture_units_[i].coord_attrib_.buffer_obj_ == (i == 1 ? 8 : 0));
    }
    REQUIRE(context.bind_buffer(GL_ARRAY_BUFFER_EMU, 7) == 0);
    REQUIRE(context.vertex_attrib_.buffer_obj_ == 0);
    REQUIRE(context.binded_buffer(true)->handle_value() == 0);
}

TEST_CASE("GLES2 deleting a buffer detaches disabled as well as enabled attributes", "[gles2]") {
    buffer_test_context<egl_context_es2> context;
    REQUIRE(context.bind_buffer(GL_ARRAY_BUFFER_EMU, 7) == 0);
    REQUIRE(context.bind_buffer(GL_ELEMENT_ARRAY_BUFFER_EMU, 7) == 0);
    context.attributes_enabled_ = 1;
    for (auto &attribute : context.attributes_) {
        attribute.buffer_obj_ = 7;
        attribute.offset_ = 16;
        attribute.stride_ = 24;
    }
    context.attributes_.back().buffer_obj_ = 8;
    context.attrib_changed_ = false;
    context.delete_buffer(7);
    REQUIRE(context.binded_array_buffer_handle_ == 0);
    REQUIRE(context.binded_element_array_buffer_handle_ == 0);
    REQUIRE(context.attrib_changed_);
    REQUIRE(context.attributes_enabled_ == 1);
    for (std::size_t i = 0; i < context.attributes_.size(); ++i) {
        REQUIRE(context.attributes_[i].buffer_obj_ == (i + 1 == context.attributes_.size() ? 8 : 0));
        REQUIRE(context.attributes_[i].offset_ == 16);
        REQUIRE(context.attributes_[i].stride_ == 24);
    }
    REQUIRE(context.bind_buffer(GL_ARRAY_BUFFER_EMU, 7) == 0);
    REQUIRE(context.attributes_[0].buffer_obj_ == 0);
}

TEST_CASE("EGL can select native pixmaps with 32-bit colour and 16-bit depth", "[egl][pixmap]") {
    const std::int32_t attributes[] = { EGL_BUFFER_SIZE_EMU, 32, EGL_DEPTH_SIZE_EMU, 16,
        EGL_SURFACE_TYPE_EMU, EGL_PIXMAP_BIT_EMU, EGL_NONE_EMU };
    std::vector<egl_config> configs(32, egl_config(0));
    std::int32_t count = 0;
    REQUIRE(egl_choose_config_emu(nullptr, 1, const_cast<std::int32_t *>(attributes), configs.data(), configs.size(), &count));
    REQUIRE(count > 0);
    REQUIRE(configs[0].buffer_size() == 32);
    REQUIRE(configs[0].depth_bits() == 16);
    bool exact_match = false;
    for (int i = 0; i < count; i++) {
        std::uint32_t colour = 0, depth = 0, surfaces = 0;
        REQUIRE(egl_get_config_attrib_emu(nullptr, 1, configs[i].config_, EGL_BUFFER_SIZE_EMU, &colour));
        REQUIRE(egl_get_config_attrib_emu(nullptr, 1, configs[i].config_, EGL_DEPTH_SIZE_EMU, &depth));
        REQUIRE(egl_get_config_attrib_emu(nullptr, 1, configs[i].config_, EGL_SURFACE_TYPE_EMU, &surfaces));
        REQUIRE((surfaces & EGL_PIXMAP_BIT_EMU) != 0);
        REQUIRE(depth >= 16);
        exact_match |= colour == 32 && depth == 16;
    }
    REQUIRE(exact_match);
}

TEST_CASE("EGL depth and stencil minimums exclude incompatible configurations", "[egl][pixmap]") {
    for (const auto limits : { std::pair<int, int>{24, 0}, {16, 8} }) {
        std::int32_t attributes[] = { EGL_DEPTH_SIZE_EMU, limits.first,
            EGL_STENCIL_SIZE_EMU, limits.second, EGL_NONE_EMU };
        std::vector<egl_config> configs(32, egl_config(0));
        std::int32_t count = 0;
        REQUIRE(egl_choose_config_emu(nullptr, 1, attributes, configs.data(), configs.size(), &count));
        REQUIRE(count > 0);
        for (int i = 0; i < count; i++) {
            REQUIRE(configs[i].depth_bits() >= limits.first);
            REQUIRE(configs[i].stencil_bits() >= limits.second);
        }
    }
}

TEST_CASE("Pixmap readback respects Symbian row order, colours and padding", "[egl][pixmap]") {
    std::array<std::uint8_t, 36> rgba{};
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
            rgba[(y * 3 + x) * 4 + 2 - y] = 255;
            rgba[(y * 3 + x) * 4 + 3] = (y + 1) * 17;
        }
    }
    using eka2l1::epoc::display_mode;
    for (auto mode : { display_mode::color64k, display_mode::color16m,
             display_mode::color16mu, display_mode::color16ma }) {
        const int bytes = mode == display_mode::color64k ? 2 : mode == display_mode::color16m ? 3 : 4;
        const int pitch = (3 * bytes + 3) & ~3;
        std::vector<std::uint8_t> result(3 * pitch + 2, 0xA5);
        copy_egl_pixmap_pixels(rgba.data(), result.data() + 1, { 3, 3 }, pitch, mode);
        REQUIRE(result.front() == 0xA5);
        REQUIRE(result.back() == 0xA5);
        for (int y = 0; y < 3; y++) {
            const auto *row = result.data() + 1 + y * pitch;
            for (int x = 0; x < 3; x++) {
                if (bytes == 2) {
                    const std::uint16_t expected[] = {0xF800, 0x07E0, 0x001F};
                    REQUIRE((row[x * 2] | (row[x * 2 + 1] << 8)) == expected[y]);
                } else {
                    for (int c = 0; c < 3; c++) {
                        REQUIRE(row[x * bytes + c] == (c == 2 - y ? 255 : 0));
                    }
                    if (bytes == 4) {
                        REQUIRE(row[x * bytes + 3] == (mode == display_mode::color16ma ? (3 - y) * 17 : 255));
                    }
                }
            }
            for (int i = bytes * 3; i < pitch; i++) {
                REQUIRE(row[i] == 0xA5);
            }
        }
    }
}

TEST_CASE("Native bitmap writer ownership ends with its EGL surface", "[egl][pixmap]") {
    eka2l1::epoc::bitmap_cache cache(nullptr);
    eka2l1::epoc::bitwise_bitmap bitmap{};
    auto first = std::make_shared<std::function<void()>>([] {});
    auto second = std::make_shared<std::function<void()>>([] {});
    REQUIRE(cache.set_bitmap_writer(&bitmap, first));
    REQUIRE_FALSE(cache.set_bitmap_writer(&bitmap, second));
    first.reset();
    REQUIRE(cache.set_bitmap_writer(&bitmap, second));
}
