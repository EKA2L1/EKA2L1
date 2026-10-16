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
#include <limits>
#include <dispatch/libraries/gles1/def.h>
#include <dispatch/libraries/gles_shared/utils.h>
#include <dispatch/libraries/gles1/shaderman.h>

using namespace eka2l1::dispatch;

namespace {
    struct point_buffer_context : egl_context_es1 {
        ~point_buffer_context() {
            // Synthetic contexts bypass the controller's object teardown.
            objects_.clear();
        }
    };
}

// Initial state and query types follow OES_point_size_array and OES_point_sprite.
TEST_CASE("GLES1 point extensions start disabled with a float size array", "[gles1]") {
    egl_context_es1 context;
    bool enabled = true;
    REQUIRE(context.is_enabled(GL_POINT_SPRITE_OES, enabled));
    REQUIRE_FALSE(enabled);
    REQUIRE(context.is_enabled(GL_POINT_SIZE_ARRAY_OES, enabled));
    REQUIRE_FALSE(enabled);

    for (const auto query : { GL_POINT_SIZE_ARRAY_TYPE_OES, GL_POINT_SIZE_ARRAY_STRIDE_OES,
             GL_POINT_SIZE_ARRAY_BUFFER_BINDING_OES }) {
        std::int32_t result = -1;
        REQUIRE(context.get_data(nullptr, query, &result, GLES_GET_DATA_TYPE_INTEGER));
        REQUIRE(result == (query == GL_POINT_SIZE_ARRAY_TYPE_OES ? GL_FLOAT_EMU : 0));
    }
    REQUIRE(context.point_size_attrib_.offset_ == 0);
    for (const auto &unit : context.texture_units_) {
        REQUIRE_FALSE(unit.point_coord_replace_);
    }

    for (std::uint32_t unit = 0; unit < GLES1_EMU_MAX_TEXTURE_COUNT; ++unit) {
        context.active_texture_unit_ = unit;
        float coords[4]{};
        REQUIRE(context.get_data(nullptr, GL_CURRENT_TEXTURE_COORDS_EMU, coords, GLES_GET_DATA_TYPE_FLOAT));
        REQUIRE(coords[0] == 0.0f);
        REQUIRE(coords[1] == 0.0f);
        REQUIRE(coords[2] == 0.0f);
        REQUIRE(coords[3] == 1.0f);
    }

    REQUIRE(context.enable(GL_POINT_SPRITE_OES));
    REQUIRE(context.is_enabled(GL_POINT_SPRITE_OES, enabled));
    REQUIRE(enabled);
    REQUIRE(context.disable(GL_POINT_SPRITE_OES));
    REQUIRE(context.is_enabled(GL_POINT_SPRITE_OES, enabled));
    REQUIRE_FALSE(enabled);
}

TEST_CASE("GLES1 point size queries preserve pixel units and fixed point precision", "[gles1]") {
    egl_context_es1 context;
    float floating = 0.0f;
    std::int32_t fixed = 0;
    std::int32_t integer = 0;
    REQUIRE(context.get_data(nullptr, GL_POINT_SIZE_EMU, &floating, GLES_GET_DATA_TYPE_FLOAT));
    REQUIRE(floating == 1.0f);
    context.point_size_ = 2.5f;
    REQUIRE(context.get_data(nullptr, GL_POINT_SIZE_EMU, &floating, GLES_GET_DATA_TYPE_FLOAT));
    REQUIRE(floating == 2.5f);
    REQUIRE(context.get_data(nullptr, GL_POINT_SIZE_EMU, &fixed, GLES_GET_DATA_TYPE_FIXED));
    REQUIRE(fixed == 163840);
    REQUIRE(context.get_data(nullptr, GL_POINT_SIZE_EMU, &integer, GLES_GET_DATA_TYPE_INTEGER));
    REQUIRE(integer == 3);
    REQUIRE(context.get_data(nullptr, GL_POINT_SIZE_EMU, &integer, GLES_GET_DATA_TYPE_BOOLEAN));
    REQUIRE(integer == 1);
    context.point_size_ = 7.0f;
    REQUIRE(context.get_data(nullptr, GL_POINT_SIZE_EMU, &integer, GLES_GET_DATA_TYPE_INTEGER));
    REQUIRE(integer == 7);
}

TEST_CASE("GLES1 shader keys distinguish texture units across point and triangle draws", "[gles1]") {
    for (const std::uint64_t state : { std::uint64_t(0),
             std::uint64_t(egl_context_es1::VERTEX_STATE_DRAW_POINTS),
             std::uint64_t(egl_context_es1::VERTEX_STATE_DRAW_POINTS | egl_context_es1::VERTEX_STATE_CLIENT_POINT_SIZE_ARRAY) }) {
        const auto no_texture = gles1_vertex_shader_key(state, 0);
        const auto first_texture = gles1_vertex_shader_key(state, 1);
        const auto two_textures = gles1_vertex_shader_key(state, 5);
        REQUIRE(no_texture != first_texture);
        REQUIRE(first_texture != two_textures);
        REQUIRE(two_textures != no_texture);
    }
    REQUIRE(gles1_vertex_shader_key(0, 1) == gles1_vertex_shader_key(egl_context_es1::VERTEX_STATE_CLIENT_POINT_SIZE_ARRAY, 1));
    REQUIRE(gles1_vertex_shader_key(0, 1) != gles1_vertex_shader_key(egl_context_es1::VERTEX_STATE_DRAW_POINTS, 1));
    REQUIRE(gles1_vertex_shader_key(egl_context_es1::VERTEX_STATE_DRAW_POINTS, 1)
        != gles1_vertex_shader_key(egl_context_es1::VERTEX_STATE_DRAW_POINTS | egl_context_es1::VERTEX_STATE_CLIENT_POINT_SIZE_ARRAY, 1));
}

TEST_CASE("GLES1 sprite coordinate replacement is queried per texture unit", "[gles1]") {
    egl_context_es1 context;
    context.active_texture_unit_ = 1;
    context.active_client_texture_unit_ = 2;
    REQUIRE(context.texture_units_[1].set_point_coord_replace(1.0) == 0);
    for (std::size_t unit = 0; unit < GLES1_EMU_MAX_TEXTURE_COUNT; ++unit) {
        std::int32_t result = -1;
        REQUIRE(context.texture_units_[unit].get_env(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES,
            &result, GLES_GET_DATA_TYPE_INTEGER) == 0);
        REQUIRE(result == (unit == 1));
        float floating = -1.0f;
        REQUIRE(context.texture_units_[unit].get_env(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES,
            &floating, GLES_GET_DATA_TYPE_FLOAT) == 0);
        REQUIRE(floating == (unit == 1 ? 1.0f : 0.0f));
        gl_fixed fixed = -1;
        REQUIRE(context.texture_units_[unit].get_env(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES,
            &fixed, GLES_GET_DATA_TYPE_FIXED) == 0);
        REQUIRE(fixed == (unit == 1));
    }
    std::int32_t unchanged = 123;
    REQUIRE(context.texture_units_[1].get_env(GL_POINT_SPRITE_OES, GL_TEX_ENV_MODE_EMU,
        &unchanged, GLES_GET_DATA_TYPE_INTEGER) == GL_INVALID_ENUM);
    REQUIRE(unchanged == 123);
    REQUIRE(context.texture_units_[1].get_env(GL_TEX_ENV_EMU, GL_COORD_REPLACE_OES,
        &unchanged, GLES_GET_DATA_TYPE_INTEGER) == GL_INVALID_ENUM);
    REQUIRE(unchanged == 123);
}

TEST_CASE("GLES1 texture environment getters preserve core enum and numeric conversions", "[gles1]") {
    gles1_texture_unit unit;
    const std::pair<std::uint32_t, std::int32_t> defaults[] = {
        { GL_TEX_ENV_MODE_EMU, GL_MODULATE_EMU },
        { GL_COMBINE_RGB_EMU, GL_MODULATE_EMU },
        { GL_COMBINE_ALPHA_EMU, GL_MODULATE_EMU },
        { GL_SRC0_RGB_EMU, GL_TEXTURE_EMU },
        { GL_SRC1_RGB_EMU, GL_PREVIOUS_EMU },
        { GL_SRC2_RGB_EMU, GL_CONSTANT_EMU },
        { GL_SRC0_ALPHA_EMU, GL_TEXTURE_EMU },
        { GL_SRC1_ALPHA_EMU, GL_PREVIOUS_EMU },
        { GL_SRC2_ALPHA_EMU, GL_CONSTANT_EMU },
        { GL_OPERAND0_RGB_EMU, GL_SRC_COLOR_EMU },
        { GL_OPERAND1_RGB_EMU, GL_SRC_COLOR_EMU },
        { GL_OPERAND2_RGB_EMU, GL_SRC_ALPHA_EMU },
        { GL_OPERAND0_ALPHA_EMU, GL_SRC_ALPHA_EMU },
        { GL_OPERAND1_ALPHA_EMU, GL_SRC_ALPHA_EMU },
        { GL_OPERAND2_ALPHA_EMU, GL_SRC_ALPHA_EMU },
        { GL_RGB_SCALE_EMU, 1 },
        { GL_ALPHA_SCALE_EMU, 1 }
    };
    for (const auto &[pname, expected] : defaults) {
        std::int32_t result = -1;
        REQUIRE(unit.get_env(GL_TEX_ENV_EMU, pname, &result, GLES_GET_DATA_TYPE_INTEGER) == 0);
        REQUIRE(result == expected);
    }
    gl_fixed fixed = 0;
    REQUIRE(unit.get_env(GL_TEX_ENV_EMU, GL_RGB_SCALE_EMU, &fixed, GLES_GET_DATA_TYPE_FIXED) == 0);
    REQUIRE(fixed == 65536);
    REQUIRE(unit.get_env(GL_TEX_ENV_EMU, GL_TEX_ENV_MODE_EMU, &fixed, GLES_GET_DATA_TYPE_FIXED) == 0);
    REQUIRE(fixed == GL_MODULATE_EMU);
    unit.env_colors_[0] = -1.0f;
    unit.env_colors_[1] = 0.0f;
    unit.env_colors_[2] = 1.0f;
    unit.env_colors_[3] = 0.5f;
    std::int32_t colors[4]{};
    REQUIRE(unit.get_env(GL_TEX_ENV_EMU, GL_TEX_ENV_COLOR_EMU, colors, GLES_GET_DATA_TYPE_INTEGER) == 0);
    REQUIRE(colors[0] == std::numeric_limits<std::int32_t>::min());
    REQUIRE(colors[1] == 0);
    REQUIRE(colors[2] == std::numeric_limits<std::int32_t>::max());
    REQUIRE(unit.get_env(GL_TEX_ENV_EMU, GL_TEX_ENV_COLOR_EMU, colors, GLES_GET_DATA_TYPE_FIXED) == 0);
    REQUIRE(colors[0] == -65536);
    REQUIRE(colors[1] == 0);
    REQUIRE(colors[2] == 65536);
    REQUIRE(colors[3] == 32768);
}

TEST_CASE("GLES1 large point sizes saturate integer query results", "[gles1]") {
    egl_context_es1 context;
    context.point_size_ = std::numeric_limits<float>::max();
    std::int32_t result = 0;
    REQUIRE(context.get_data(nullptr, GL_POINT_SIZE_EMU, &result, GLES_GET_DATA_TYPE_INTEGER));
    REQUIRE(result == std::numeric_limits<std::int32_t>::max());
    REQUIRE(context.get_data(nullptr, GL_POINT_SIZE_EMU, &result, GLES_GET_DATA_TYPE_FIXED));
    REQUIRE(result == std::numeric_limits<std::int32_t>::max());
}

TEST_CASE("GLES1 rejects unavailable point size buffers before preparing a draw", "[gles1]") {
    point_buffer_context context;
    egl_controller controller(nullptr);
    context.associated_thread_uid_ = 7;
    gles_driver_object_instance vertex_buffer = std::make_unique<gles_driver_buffer>(context);
    vertex_buffer->assign_handle(1);
    context.vertex_attrib_.buffer_obj_ = static_cast<std::uint32_t>(context.objects_.add(vertex_buffer));
    context.point_size_attrib_.buffer_obj_ = 999;
    context.vertex_statuses_ |= egl_context_es1::VERTEX_STATE_CLIENT_VERTEX_ARRAY
        | egl_context_es1::VERTEX_STATE_CLIENT_POINT_SIZE_ARRAY;
    REQUIRE_FALSE(context.prepare_for_draw(nullptr, controller, nullptr, 0, 1, eka2l1::drivers::graphics_primitive_mode::points));
    REQUIRE(controller.pop_error(7) == GL_INVALID_OPERATION);
    REQUIRE(context.input_desc_ == 0);
}

TEST_CASE("GLES1 coordinate replacement rejects invalid booleans without changing state", "[gles1]") {
    gles1_texture_unit unit;
    REQUIRE(unit.set_point_coord_replace(1.0) == 0);
    for (const double invalid : { -1.0, 0.5, 2.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() }) {
        REQUIRE(unit.set_point_coord_replace(invalid) == GL_INVALID_ENUM);
        REQUIRE(unit.point_coord_replace_);
    }
    REQUIRE(unit.set_point_coord_replace(0.0) == 0);
    REQUIRE_FALSE(unit.point_coord_replace_);
}

namespace {
    class point_draw_driver : public eka2l1::drivers::graphics_driver {
        eka2l1::drivers::handle next_ = 1;

    public:
        std::vector<eka2l1::drivers::input_descriptor> inputs;
        point_draw_driver() : graphics_driver(eka2l1::drivers::graphic_api::opengl) {}
        void run() override {}
        void abort() override {}
        void update_bitmap(eka2l1::drivers::handle, std::size_t, const eka2l1::vec2 &, const eka2l1::vec2 &, const void *, std::size_t) override {}
        void set_viewport(const eka2l1::rect &) override {}
        void update_surface(void *) override {}
        void update_surface_size(const eka2l1::vec2 &) override {}
        void set_upscale_shader(const std::string &) override {}
        std::string get_active_upscale_shader() const override { return {}; }
        bool support_extension(eka2l1::drivers::graphics_driver_extension) override { return false; }
        bool query_extension_value(eka2l1::drivers::graphics_driver_extension_query query, void *data) override {
            if (query != eka2l1::drivers::graphics_driver_extension_query_point_size_range) {
                return false;
            }
            auto *range = static_cast<float*>(data);
            range[0] = 1.0f;
            range[1] = 64.0f;
            return true;
        }
        void submit_command_list(eka2l1::drivers::command_list &commands) override {
            using namespace eka2l1::drivers;
            for (std::size_t i = 0; i < commands.size_; ++i) {
                auto &command = commands.base_[i];
                if (command.opcode_ == graphics_driver_create_input_descriptor) {
                    auto *data = reinterpret_cast<input_descriptor*>(command.data_[0]);
                    inputs.assign(data, data + command.data_[1]);
                    if (command.data_[2]) {
                        delete[] reinterpret_cast<std::uint8_t*>(data);
                    } else {
                        *reinterpret_cast<handle*>(command.data_[3]) = next_++;
                    }
                } else if (command.opcode_ == graphics_driver_create_shader_module || command.opcode_ == graphics_driver_create_shader_program) {
                    *reinterpret_cast<handle*>(command.data_[3]) = next_++;
                }
                if (command.status_) {
                    *command.status_ = 0;
                }
            }
            delete[] commands.base_;
        }
    };
}

TEST_CASE("GLES1 point arrays retain VBO offsets and are ignored for triangle draws", "[gles1]") {
    using namespace eka2l1::drivers;
    for (const auto type : { GL_FLOAT_EMU, GL_FIXED_EMU }) {
        point_draw_driver driver;
        egl_controller controller(&driver);
        point_buffer_context context;
        context.associated_thread_uid_ = 9;
        gles_driver_object_instance buffer = std::make_unique<gles_driver_buffer>(context);
        buffer->assign_handle(42);
        const auto name = static_cast<std::uint32_t>(context.objects_.add(buffer));
        context.vertex_attrib_ = { 3, GL_FLOAT_EMU, 16, 0, name };
        context.point_size_attrib_ = { 1, static_cast<std::uint32_t>(type), 16, 12, name };
        context.vertex_statuses_ |= egl_context_es1::VERTEX_STATE_CLIENT_VERTEX_ARRAY | egl_context_es1::VERTEX_STATE_CLIENT_POINT_SIZE_ARRAY;
        REQUIRE(context.prepare_for_draw(&driver, controller, nullptr, 3, 5, graphics_primitive_mode::points));
        REQUIRE(driver.inputs.size() == 2);
        REQUIRE(driver.inputs[1].location == GLES1_EMU_POINT_SIZE_ATTRIBUTE);
        REQUIRE(driver.inputs[1].offset == 60);
        REQUIRE(driver.inputs[1].stride == 16);
        REQUIRE(driver.inputs[1].buffer_slot == driver.inputs[0].buffer_slot);
        REQUIRE_FALSE(driver.inputs[1].is_normalized());
        input_descriptor expected;
        expected.set_format(1, type == GL_FIXED_EMU ? data_format::sint : data_format::sfloat);
        REQUIRE(driver.inputs[1].format == expected.format);

        context.point_size_attrib_.buffer_obj_ = 999;
        REQUIRE(context.prepare_for_draw(&driver, controller, nullptr, 3, 5, graphics_primitive_mode::triangles));
        auto commands = context.cmd_builder_.retrieve_command_list();
        driver.submit_command_list(commands);
        REQUIRE(driver.inputs.size() == 1);
        REQUIRE_FALSE(context.prepare_for_draw(&driver, controller, nullptr, 3, 5, graphics_primitive_mode::points));
        REQUIRE(controller.pop_error(9) == GL_INVALID_OPERATION);
    }
}

TEST_CASE("GLES1 point parameter state follows the ES 1.1 defaults and conversions", "[gles1]") {
    point_draw_driver driver;
    egl_context_es1 context;
    float result[3]{};
    REQUIRE(context.get_data(&driver, GL_POINT_DISTANCE_ATTENUATION_EMU, result, GLES_GET_DATA_TYPE_FLOAT));
    REQUIRE(result[0] == 1.0f);
    REQUIRE(result[1] == 0.0f);
    REQUIRE(result[2] == 0.0f);
    REQUIRE(context.get_data(&driver, GL_POINT_SIZE_MIN_EMU, result, GLES_GET_DATA_TYPE_FLOAT));
    REQUIRE(result[0] == 0.0f);
    REQUIRE(context.get_data(&driver, GL_POINT_SIZE_MAX_EMU, result, GLES_GET_DATA_TYPE_FLOAT));
    REQUIRE(result[0] == 64.0f);
    REQUIRE(context.get_data(&driver, GL_POINT_FADE_THRESHOLD_SIZE_EMU, result, GLES_GET_DATA_TYPE_FLOAT));
    REQUIRE(result[0] == 1.0f);

    const float attenuation[3] = { 0.5f, 0.25f, 0.125f };
    REQUIRE(context.set_point_parameter(GL_POINT_DISTANCE_ATTENUATION_EMU, attenuation) == 0);
    gl_fixed fixed[3]{};
    REQUIRE(context.get_data(&driver, GL_POINT_DISTANCE_ATTENUATION_EMU, fixed, GLES_GET_DATA_TYPE_FIXED));
    REQUIRE(fixed[0] == 32768);
    REQUIRE(fixed[1] == 16384);
    REQUIRE(fixed[2] == 8192);
    REQUIRE(context.get_data(&driver, GL_POINT_DISTANCE_ATTENUATION_EMU, fixed, GLES_GET_DATA_TYPE_BOOLEAN));
    REQUIRE(fixed[0] == 1);
    REQUIRE(fixed[1] == 1);
    REQUIRE(fixed[2] == 1);
}

TEST_CASE("GLES1 point limits reject negative values and preserve the native size range", "[gles1]") {
    point_draw_driver driver;
    egl_context_es1 context;
    for (const auto pname : { GL_POINT_SIZE_MIN_EMU, GL_POINT_SIZE_MAX_EMU, GL_POINT_FADE_THRESHOLD_SIZE_EMU }) {
        const float value = 2.5f;
        REQUIRE(context.set_point_parameter(pname, &value) == 0);
        const float invalid = -0.5f;
        REQUIRE(context.set_point_parameter(pname, &invalid) == GL_INVALID_VALUE);
        float result = 0.0f;
        REQUIRE(context.get_data(&driver, pname, &result, GLES_GET_DATA_TYPE_FLOAT));
        REQUIRE(result == value);
        gl_fixed fixed = 0;
        REQUIRE(context.get_data(&driver, pname, &fixed, GLES_GET_DATA_TYPE_FIXED));
        REQUIRE(fixed == 163840);
    }
    float range[2]{};
    REQUIRE(context.get_data(&driver, GL_ALIASED_POINT_SIZE_RANGE_EMU, range, GLES_GET_DATA_TYPE_FLOAT));
    REQUIRE(range[0] == 1.0f);
    REQUIRE(range[1] == 64.0f);
    const float unused = 1.0f;
    REQUIRE(context.set_point_parameter(GL_POINT_SIZE_EMU, &unused) == GL_INVALID_ENUM);
}

TEST_CASE("Paletted textures decode rows, high nibbles and successive mip levels", "[gles1][palette]") {
    for (const auto format : { GL_PALETTE4_RGB8_OES_EMU, GL_PALETTE4_RGBA8_OES_EMU,
             GL_PALETTE4_R5_G6_B5_OES_EMU, GL_PALETTE4_RGBA4_OES_EMU, GL_PALETTE4_RGB5_A1_OES_EMU,
             GL_PALETTE8_RGB8_OES_EMU, GL_PALETTE8_RGBA8_OES_EMU,
             GL_PALETTE8_R5_G6_B5_OES_EMU, GL_PALETTE8_RGBA4_OES_EMU, GL_PALETTE8_RGB5_A1_OES_EMU }) {
        CAPTURE(format);
        const bool four_bit = format <= GL_PALETTE4_RGB5_A1_OES_EMU;
        const auto kind = (format - GL_PALETTE4_RGB8_OES_EMU) % 5;
        const std::size_t bytes = kind == 0 ? 3 : (kind == 1 ? 4 : 2);
        std::vector<std::uint8_t> source((four_bit ? 16 : 256) * bytes);
        for (std::size_t index = 0; index < source.size() / bytes; ++index) {
            for (std::size_t channel = 0; channel < bytes; ++channel) {
                source[index * bytes + channel] = static_cast<std::uint8_t>(index * 4 + channel);
            }
        }
        if (four_bit) {
            source.insert(source.end(), { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xB0 });
        } else {
            source.insert(source.end(), { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 });
        }
        std::vector<std::uint8_t> decoded;
        std::vector<std::size_t> sizes;
        eka2l1::drivers::texture_format texture_format;
        eka2l1::drivers::texture_data_type data_type;
        std::uint32_t gl_format = 0;
        REQUIRE(decompress_palette_data(decoded, sizes, source.data(), source.size(), 4, 2, format, 3,
            texture_format, data_type, gl_format));
        REQUIRE(sizes == std::vector<std::size_t>{ 8 * bytes, 2 * bytes, bytes });
        std::vector<std::uint8_t> expected;
        for (std::size_t index = 1; index <= 11; ++index) {
            expected.insert(expected.end(), source.begin() + index * bytes, source.begin() + (index + 1) * bytes);
        }
        REQUIRE(decoded == expected);
        for (const auto size : { std::size_t(0), source.size() - 1, source.size() + 1 }) {
            REQUIRE_FALSE(decompress_palette_data(decoded, sizes, source.data(), size, 4, 2, format, 3,
                texture_format, data_type, gl_format));
        }
        REQUIRE_FALSE(decompress_palette_data(decoded, sizes, source.data(), source.size(), 4, 2, format, 14,
            texture_format, data_type, gl_format));
        source.resize((four_bit ? 16 : 256) * bytes);
        source.push_back(four_bit ? 0x10 : 1);
        source.push_back(four_bit ? 0x20 : 2);
        REQUIRE(decompress_palette_data(decoded, sizes, source.data(), source.size(), 1, 1, format, 2,
            texture_format, data_type, gl_format));
        REQUIRE(sizes == std::vector<std::size_t>{ bytes, bytes });
        REQUIRE(decoded == std::vector<std::uint8_t>(source.begin() + bytes, source.begin() + 3 * bytes));
    }
}

TEST_CASE("EGL colour configuration channel sizes match their buffer sizes", "[egl]") {
    for (const auto api : { egl_config::EGL_TARGET_CONTEXT_ES11, egl_config::EGL_TARGET_CONTEXT_ES2,
             egl_config::EGL_TARGET_CONTEXT_VG }) {
        for (const auto bits : { 16, 24, 32 }) {
            egl_config config(0);
            config.set_buffer_size(bits);
            config.set_target_context_version(api);
            REQUIRE(config.buffer_size() == bits);
            REQUIRE(config.red_bits() == (bits == 16 ? 5 : 8));
            REQUIRE(config.green_bits() == (bits == 16 ? 6 : 8));
            REQUIRE(config.blue_bits() == (bits == 16 ? 5 : 8));
            REQUIRE(config.alpha_bits() == (bits == 32 ? 8 : 0));
        }
    }
}

TEST_CASE("GLES texture copies validate the defined image rather than the last upload", "[gles1][texture-copy]") {
    egl_context_es1 context;
    egl_controller controller(nullptr);
    egl_config config(0);
    config.set_buffer_size(32);
    egl_surface surface(nullptr, nullptr, eka2l1::vec2(640, 360), 7, config);
    context.read_surface_ = context.draw_surface_ = &surface;
    gles_driver_texture texture(context);
    REQUIRE(texture.try_bind(GL_TEXTURE_2D_EMU) == 0);
    texture.assign_handle(42);
    texture.define_image(GL_TEXTURE_2D_EMU, 0, eka2l1::vec2(64, 32), GL_RGB_EMU);
    texture.define_image(GL_TEXTURE_2D_EMU, 1, eka2l1::vec2(7, 5), GL_RGB_EMU);
    auto copy = [&](int level, eka2l1::vec2 offset, eka2l1::vec2 size) {
        return texture.copy_sub_image(controller, nullptr, GL_TEXTURE_2D_EMU, level, offset, {0, 0}, size);
    };
    REQUIRE(copy(0, {32, 16}, {32, 16}) == 0);
    REQUIRE(copy(1, {6, 4}, {1, 1}) == 0);
    REQUIRE(copy(1, {6, 4}, {2, 1}) == GL_INVALID_VALUE);
    REQUIRE(copy(2, {0, 0}, {1, 1}) == GL_INVALID_OPERATION);
    REQUIRE(copy(-1, {0, 0}, {1, 1}) == GL_INVALID_VALUE);
    REQUIRE(copy(32, {0, 0}, {1, 1}) == GL_INVALID_VALUE);
    REQUIRE(copy(0, {-1, 0}, {1, 1}) == GL_INVALID_VALUE);
    REQUIRE(copy(0, {0, 0}, {-1, 1}) == GL_INVALID_VALUE);
    REQUIRE(copy(0, {std::numeric_limits<int>::max(), 0}, {1, 1}) == GL_INVALID_VALUE);
    REQUIRE(copy(0, {64, 32}, {0, 0}) == 0);
    REQUIRE(texture.copy_sub_image(controller, nullptr, GL_TEXTURE_CUBE_MAP_POSITIVE_X_EMU, 0, {0, 0}, {0, 0}, {1, 1}) == GL_INVALID_ENUM);
    texture.define_image(GL_TEXTURE_2D_EMU, 0, {64, 32}, GL_ETC1_RGB8_OES_EMU, true);
    REQUIRE(copy(0, {0, 0}, {1, 1}) == GL_INVALID_OPERATION);
    auto commands = context.cmd_builder_.retrieve_command_list();
    REQUIRE(commands.size_ == 2);
    delete[] commands.base_;
}

TEST_CASE("GLES copies preserve alpha compatibility and generated mip dimensions", "[gles1][texture-copy]") {
    egl_context_es1 context;
    egl_controller controller(nullptr);
    egl_config config(0);
    config.set_buffer_size(16);
    egl_surface surface(nullptr, nullptr, {640, 360}, 7, config);
    context.read_surface_ = context.draw_surface_ = &surface;
    gles_driver_texture texture(context);
    REQUIRE(texture.try_bind(GL_TEXTURE_2D_EMU) == 0);
    texture.assign_handle(42);
    auto copy = [&](int level, eka2l1::vec2 size) {
        return texture.copy_sub_image(controller, nullptr, GL_TEXTURE_2D_EMU, level, {0, 0}, {0, 0}, size);
    };
    for (auto format : { GL_RGBA_EMU, GL_ALPHA_EMU, GL_LUMINANCE_ALPHA_EMU }) {
        texture.define_image(GL_TEXTURE_2D_EMU, 0, {32, 16}, format);
        REQUIRE(copy(0, {1, 1}) == GL_INVALID_OPERATION);
    }
    texture.define_image(GL_TEXTURE_2D_EMU, 0, {4096, 1}, GL_RGB_EMU);
    texture.set_mipmap_generated(true);
    REQUIRE(copy(12, {1, 1}) == 0);
    REQUIRE(copy(12, {2, 1}) == GL_INVALID_VALUE);
    REQUIRE(copy(13, {1, 1}) == GL_INVALID_VALUE);
    auto commands = context.cmd_builder_.retrieve_command_list();
    REQUIRE(commands.size_ == 1);
    delete[] commands.base_;
}

TEST_CASE("GLES copies use framebuffer scale without scaling ordinary texture offsets", "[gles1][texture-copy]") {
    egl_context_es1 context;
    egl_controller controller(nullptr);
    egl_config config(0);
    config.set_buffer_size(32);
    egl_surface surface(nullptr, nullptr, {640, 360}, 7, config);
    surface.current_scale_ = 2.0f;
    context.read_surface_ = context.draw_surface_ = &surface;
    gles_driver_texture texture(context);
    REQUIRE(texture.try_bind(GL_TEXTURE_2D_EMU) == 0);
    texture.assign_handle(42);
    texture.define_image(GL_TEXTURE_2D_EMU, 0, {64, 32}, GL_RGB_EMU);
    REQUIRE(texture.copy_sub_image(controller, nullptr, GL_TEXTURE_2D_EMU, 0, {5, 7}, {11, 13}, {4, 6}) == 0);
    REQUIRE(texture.copy_sub_image(controller, nullptr, GL_TEXTURE_2D_EMU, 0, {0, 0},
        {std::numeric_limits<int>::max(), 0}, {1, 1}) == 0);
    REQUIRE(texture.copy_sub_image(controller, nullptr, GL_TEXTURE_2D_EMU, 0, {0, 0},
        {std::numeric_limits<int>::min(), 0}, {1, 1}) == 0);
    auto commands = context.cmd_builder_.retrieve_command_list();
    REQUIRE(commands.size_ == 1);
    const auto &copy = commands.base_[0];
    REQUIRE(copy.opcode_ == eka2l1::drivers::graphics_driver_copy_framebuffer_to_texture);
    REQUIRE(copy.data_[0] == 7);
    REQUIRE(copy.data_[1] == 42);
    REQUIRE(copy.data_[2] == 2);
    REQUIRE(copy.data_[3] == (std::uint64_t(26) << 32 | 22));
    REQUIRE(copy.data_[4] == (std::uint64_t(12) << 32 | 8));
    REQUIRE(copy.data_[5] == (std::uint64_t(7) << 32 | 5));
    REQUIRE(copy.data_[6] == (std::uint64_t(6) << 32 | 4));
    REQUIRE(copy.data_[7] == 0);
    delete[] commands.base_;
}
