/*
 * Copyright (c) 2023 EKA2L1 Team
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <loader/nvg.h>
#include <common/log.h>

#include <map>

namespace eka2l1::loader {
    static constexpr std::uint32_t NVG_TYPE_OFFSET = 6;
    static constexpr std::uint32_t NVG_VERSION_OFFSET = 3;
    static constexpr std::uint32_t NVG_HEADERSIZE_OFFSET = 4;
    static constexpr std::uint32_t NVG_PATH_DATATYPE_OFFSET = 26;   // int16

    // Unused mostly, scale and bias is hardcoded depends on path type (8/16/32 bits)
    static constexpr std::uint32_t NVG_PATH_DATA_SCALE_OFFSET = 28; // float
    static constexpr std::uint32_t NVG_PATH_DATA_BIAS_OFFSET = 32; // float
    static constexpr std::uint32_t NVG_VIEWPORT_INFO_OFFSET = 36; // float (x,y,w,h)

    enum nvg_direct_command_opcode {
        NVG_DIRECT_COMMAND_SET_FILL_PAINT = 4,
        NVG_DIRECT_COMMAND_SET_STROKE_PAINT = 5,
        NVG_DIRECT_COMMAND_SET_COLOR_RAMP = 6,
        NVG_DIRECT_COMMAND_DRAW_PATH = 7,
        NVG_DIRECT_COMMAND_SET_TRANSFORMATION = 8,
        NVG_DIRECT_COMMAND_SET_STROKE_WIDTH = 9,
        NVG_DIRECT_COMMAND_SET_STROKE_LINE_JOIN_CAP = 10,
        NVG_DIRECT_COMMAND_SET_STROKE_MITER_LIMIT = 11
    };

    enum nvg_brush_type {
        NVG_BRUSH_FLAT_COLOR = 1,
        NVG_BRUSH_LINEAR_GRAD = 2,
        NVG_BRUSH_RADIAL_GRAD = 3,
        NVG_BRUSH_COLOR_RAMP_SET = 4
    };

    enum nvg_path_data_type {
        NVG_PATH_EIGHT_BIT_DECODING = 1,
        NVG_PATH_SIXTEEN_BIT_DECODING = 2,
        NVG_PATH_THIRTYTWO_BIT_DECODING = 3
    };

    enum vg_path_segment_type {
        VG_CLOSE_PATH                               = ( 0 << 1),
        VG_MOVE_TO                                  = ( 1 << 1),
        VG_LINE_TO                                  = ( 2 << 1),
        VG_HLINE_TO                                 = ( 3 << 1),
        VG_VLINE_TO                                 = ( 4 << 1),
        VG_QUAD_TO                                  = ( 5 << 1),
        VG_CUBIC_TO                                 = ( 6 << 1),
        VG_SQUAD_TO                                 = ( 7 << 1),
        VG_SCUBIC_TO                                = ( 8 << 1),
        VG_SCCWARC_TO                               = ( 9 << 1),
        VG_SCWARC_TO                                = (10 << 1),
        VG_LCCWARC_TO                               = (11 << 1),
        VG_LCWARC_TO                                = (12 << 1),
    };

    enum nvg_transform_set_type {
        NVG_TRANSFORM_SET_COMPLETE = 0,
        NVG_TRANSFORM_SET_SCALING = 2,
        NVG_TRANSFORM_SET_SHEARING = 4,
        NVG_TRANSFORM_SET_ROTATION = 8,
        NVG_TRANSFORM_SET_TRANSLATION = 16
    };

    struct nvg_color_ramp_stop_info {
        float offset_;
        float color_[4];
    };

    struct nvg_brush {
        nvg_brush_type brush_type_ = NVG_BRUSH_FLAT_COLOR;

        std::uint32_t color_[4];
        float extra_[5];
        float transform_[6];

        bool has_transform_ = false;
        bool change_flushed_ = false;

        std::uint32_t version_ = 0;
        std::vector<nvg_color_ramp_stop_info> ramps_;

        explicit nvg_brush() {
            // Not standard but the most normal
            color_[0] = color_[1] = color_[2] = color_[3] = 255;
        }
    };

    struct nvg_state {
        nvg_brush fill_brush_;
        nvg_brush stroke_brush_;

        nvg_path_data_type path_datatype_ = NVG_PATH_SIXTEEN_BIT_DECODING;

        float transform_matrix_[6];
        bool no_transform_matrix_ = true;

        float stroke_width_ = 1.0f;
        float stroke_miter_limit = 4.0f;
    };

    void uint32_to_float_rgba(const std::uint32_t rgba, std::uint32_t *rgba_sep) {
        rgba_sep[0] = ((rgba >> 24) & 0xFF);
        rgba_sep[1] = ((rgba >> 16) & 0xFF);
        rgba_sep[2] = ((rgba >> 8) & 0xFF);
    }

    std::string emit_gradient_stops(nvg_brush &brush) {
        std::string result;

        for (const nvg_color_ramp_stop_info &info: brush.ramps_) {
            result += fmt::format("\t<stop offset=\"{}\" stop-color=\"#{:02X}{:02X}{:02X}\" stop-opacity=\"{}\" />\n", info.offset_ ,
                static_cast<int>(info.color_[0] * 255.0f), static_cast<int>(info.color_[1] * 255.0f), static_cast<int>(info.color_[2] * 255.0f),
                info.color_[3]);
        }

        return result;
    }

    std::string emit_complex_brush_style(nvg_brush &brush, const std::string &prefix) {
        // Note that gradient transform matrix needs to be transposed from OpenVG model (column-majored)
        // Just my guess though. Here is source: https://github.com/SymbianSource/oss.FCL.sf.mw.svgt/blob/a6e9f61716263f102bd673b7b9161c4519b95dae/svgtopt/nvgdecoder/src/nvg.cpp#L1036
        if (brush.brush_type_ == NVG_BRUSH_LINEAR_GRAD) {
            std::string result = fmt::format(R"A(
<linearGradient
    id = "{}" 
    x1 = "{}" 
    x2 = "{}" 
    y1 = "{}" 
    y2 = "{}" 
    gradientUnits = "userSpaceOnUse"
    {}
>
)A",
                prefix + fmt::format("{}", brush.version_), brush.extra_[0], brush.extra_[2], brush.extra_[1], brush.extra_[3],
                brush.has_transform_ ? fmt::format("gradientTransform = \"matrix({} {} {} {} {} {})\"", brush.transform_[0], brush.transform_[3], brush.transform_[1], brush.transform_[4], brush.transform_[2], brush.transform_[5]) : "");

            result += emit_gradient_stops(brush);
            result += "</linearGradient>\n";

            return result;
        } else if (brush.brush_type_ == NVG_BRUSH_RADIAL_GRAD) {
            std::string result = fmt::format(R"A(
<radialGradient
    id = "{}" 
    cx = "{}" 
    cy = "{}" 
    fx = "{}" 
    fy = "{}"
    fr = "{}" 
    gradientUnits = "userSpaceOnUse"
    {}
>
)A",
                prefix + fmt::format("{}", brush.version_), brush.extra_[0], brush.extra_[1], brush.extra_[2], brush.extra_[3], brush.extra_[4],
                brush.has_transform_ ? fmt::format("gradientTransform = \"matrix({} {} {} {} {} {})\"", brush.transform_[0], brush.transform_[3], brush.transform_[1], brush.transform_[4], brush.transform_[2], brush.transform_[5]) : "");

            result += emit_gradient_stops(brush);
            result += "</radialGradient>\n";

            return result;
        } else {
            return "";
        }
    }

    bool nvg_direct_command_set_color_ramp_util(nvg_brush &target_brush, std::uint32_t alpha, std::uint32_t common_data, common::ro_stream &in, std::vector<nvg_convert_error_description> &errors) {
        std::uint16_t ramp_count = static_cast<std::uint16_t>((common_data >> 16) & 0xFFFF);
        nvg_color_ramp_stop_info info;

        target_brush.ramps_.clear();

        for (std::uint16_t i = 0; i < ramp_count; i++) {
            if (in.read(&info, sizeof(nvg_color_ramp_stop_info)) != sizeof(nvg_color_ramp_stop_info)) {
                errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                return false;
            }

            info.color_[3] = alpha / 255.0f;
            target_brush.ramps_.push_back(info);
        }

        return true;
    }

    bool nvg_direct_command_set_fill_color_ramp(nvg_state &state, std::uint32_t command, common::ro_stream &in, common::wo_stream &out, std::vector<nvg_convert_error_description> &errors) {
        std::uint32_t command_data = 0;
        if (in.read(&command_data, 4) != 4) {
            errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
            return false;
        }

        return nvg_direct_command_set_color_ramp_util(state.fill_brush_, state.fill_brush_.color_[3], command_data,
            in, errors);
    }

    bool nvg_direct_command_set_paint_util(nvg_brush &brush, std::uint32_t command, bool allow_color_ramp_set, common::ro_stream &in, common::wo_stream &out, std::vector<nvg_convert_error_description> &errors) {
        std::uint32_t command_data = 0;
        if (in.read(&command_data, 4) != 4) {
            errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
            return false;
        }

        brush.color_[3] = ((command & 0xFF0000) >> 16);
        brush.change_flushed_ = false;

        std::uint32_t paint_type = command_data & 0b111;
        std::uint32_t specific_data = static_cast<std::uint16_t>((command_data >> 16) & 0xFF);
        
        switch (paint_type) {
        case NVG_BRUSH_FLAT_COLOR: {
            brush.brush_type_ = NVG_BRUSH_FLAT_COLOR;
            std::uint32_t color = 0;
            if (in.read(&color, 4) != 4) {
                errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                return false;
            }

            uint32_to_float_rgba(color, brush.color_);
            break;
        }

        case NVG_BRUSH_LINEAR_GRAD: {
            brush.brush_type_ = NVG_BRUSH_LINEAR_GRAD;
            // Read start and end point
            if (in.read(brush.extra_, 16) != 16) {
                errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                return false;
            }

            if (specific_data & 1) {
                brush.has_transform_ = true;

                if (in.read(brush.transform_, 24) != 24) {
                    errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                    return false;
                }
            } else {
                brush.has_transform_ = false;
            }

            break;
        }
        
        case NVG_BRUSH_RADIAL_GRAD: {
            brush.brush_type_ = NVG_BRUSH_RADIAL_GRAD;
            // Read start and end point
            if (in.read(brush.extra_, 20) != 20) {
                errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                return false;
            }

            if (specific_data & 1) {
                brush.has_transform_ = true;

                if (in.read(brush.transform_, 24) != 24) {
                    errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                    return false;
                }
            } else {
                brush.has_transform_ = false;
            }

            break;
        }

        case NVG_BRUSH_COLOR_RAMP_SET: {
            if (allow_color_ramp_set) {
                return nvg_direct_command_set_color_ramp_util(brush, brush.color_[3], command_data,
                    in, errors);
            } else {
                [[fallthrough]];
            }
        }

        default:
            errors.emplace_back(NVG_UNRECOGNISED_FILL_PAINT_TYPE, in.tell(), paint_type);
            return false;
        }

        return true;
    }

    bool nvg_direct_command_set_fill_paint(nvg_state &state, std::uint32_t command, common::ro_stream &in, common::wo_stream &out, std::vector<nvg_convert_error_description> &errors) {
        return nvg_direct_command_set_paint_util(state.fill_brush_, command, false, in, out, errors);
    }

    bool nvg_direct_command_set_stroke_paint(nvg_state &state, std::uint32_t command, common::ro_stream &in, common::wo_stream &out, std::vector<nvg_convert_error_description> &errors) {
        return nvg_direct_command_set_paint_util(state.stroke_brush_, command, true, in, out, errors);
    }

    template <typename T>
    bool nvg_generate_direction(common::ro_stream &in, std::string &direction, std::vector<nvg_convert_error_description> &errors,
        const std::vector<std::uint8_t> &segment_types, float scale) {
        static constexpr std::size_t ELEM_T_SIZE = sizeof(T);

        static const std::map<vg_path_segment_type, char> CORRESPOND_SVG_CMD_CHAR = {
            { VG_LINE_TO, 'L' },
            { VG_HLINE_TO, 'H' },
            { VG_VLINE_TO, 'V' },
            { VG_MOVE_TO, 'M' },
            { VG_SQUAD_TO, 'T' },   // Smooth quad
            { VG_QUAD_TO, 'Q' },
            { VG_SCUBIC_TO, 'S' },
            { VG_CUBIC_TO, 'C' },
            { VG_LCCWARC_TO, 'A' },
            { VG_SCCWARC_TO, 'A' },
            { VG_LCWARC_TO, 'A' },
            { VG_SCCWARC_TO, 'A' }
        };

        T values[6];

        for (std::uint8_t segment_type: segment_types) {
            if (segment_type == VG_CLOSE_PATH) {
                break;
            }

            auto correspond_find_res = CORRESPOND_SVG_CMD_CHAR.find(static_cast<vg_path_segment_type>(segment_type & ~1));
            if (correspond_find_res == CORRESPOND_SVG_CMD_CHAR.end()) {
                errors.emplace_back(NVG_UNKNOWN_PATH_SEGMENT_TYPE, in.tell(), segment_type & ~1);
                continue;
            }

            char result_char_res = correspond_find_res->second;
            if (segment_type & 1) {
                // Turn to relative. For SVG just lowercase
                result_char_res = static_cast<char>(std::tolower(result_char_res));
            }

            switch (segment_type) {
            case VG_HLINE_TO:
            case VG_VLINE_TO: {
                T to_pos = 0;
                        
                if (in.read(&to_pos, ELEM_T_SIZE) != ELEM_T_SIZE) {
                    errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                    return false;
                }

                direction += fmt::format("{} {} ", result_char_res, to_pos * scale);
                break;
            }

            case VG_MOVE_TO:
            case VG_LINE_TO:
            case VG_SQUAD_TO: {
                if (in.read(values, ELEM_T_SIZE * 2) != ELEM_T_SIZE * 2) {
                    errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                    return false;
                }

                direction += fmt::format("{} {} {} ", result_char_res, values[0] * scale, values[1] * scale);
                break;
            }

            case VG_SCCWARC_TO:
            case VG_SCWARC_TO:
            case VG_LCCWARC_TO:
            case VG_LCWARC_TO: {
                const bool is_counterclock_wise = (segment_type == VG_SCCWARC_TO) || (segment_type == VG_LCCWARC_TO);
                const bool is_small = (segment_type == VG_SCWARC_TO) || (segment_type == VG_SCCWARC_TO);

                if (in.read(values, ELEM_T_SIZE * 5) != ELEM_T_SIZE * 5) {
                    errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                    return false;
                }

                direction += fmt::format("{} {} {}, {}, {} {}, {} {} ", result_char_res, values[0] * scale, values[1] * scale,
                    values[2], is_small ? 0 : 1, is_counterclock_wise ? 0 : 1, values[3] * scale, values[4] * scale);

                break;
            }

            case VG_QUAD_TO:
            case VG_SCUBIC_TO: {
                if (in.read(values, ELEM_T_SIZE * 4) != ELEM_T_SIZE * 4) {
                    errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                    return false;
                }

                direction += fmt::format("{} {} {}, {} {} ", result_char_res, values[0] * scale, values[1] * scale,
                    values[2] * scale, values[3] * scale);

                break;
            }

            case VG_CUBIC_TO: {
                if (in.read(values, ELEM_T_SIZE * 6) != ELEM_T_SIZE * 6) {
                    errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                    return false;
                }

                direction += fmt::format("{} {} {}, {} {}, {} {} ", result_char_res, values[0] * scale, values[1] * scale,
                    values[2] * scale, values[3] * scale, values[4] * scale, values[5] * scale);

                break;
            }

            default:
                assert(false && "Unreachable!");
                return false;
            }
        }

        direction += "Z";
        return true;
    }

    bool nvg_direct_command_draw_path(nvg_state &state, std::uint32_t command, common::ro_stream &in, common::wo_stream &out, std::vector<nvg_convert_error_description> &errors) {
        bool do_stroke = command & 0x00010000;
        bool do_fill = command & 0x00020000;

        if (!do_stroke && !do_fill) {
            return true;
        }

        std::uint16_t segment_count = 0;
        if (in.read(&segment_count, 2) != 2) {
            errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
            return false;
        }

        std::vector<std::uint8_t> segment_types(segment_count);
        if (in.read(segment_types.data(), segment_count) != segment_count) {
            errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
            return false;
        }

        std::string direction;

        if (state.path_datatype_ == NVG_PATH_THIRTYTWO_BIT_DECODING) {
            if ((in.tell() % 4) != 0) {
                in.seek(4 - (in.tell() % 4), common::seek_where::cur);
            }

            if (!nvg_generate_direction<std::uint32_t>(in, direction, errors, segment_types, 1.0f / 65536.0f)) {
                return false;
            }
        } else {
            if ((in.tell() % 2) != 0) {
                in.seek(2 - (in.tell() % 2), common::seek_where::cur);
            }

            float scale = 0.5f;

            if (state.path_datatype_ == NVG_PATH_SIXTEEN_BIT_DECODING) {
                scale = 1.0f / 16.0f;
            }
            
            if (!nvg_generate_direction<std::uint16_t>(in, direction, errors, segment_types, scale)) {
                return false;
            }
        }

        static const char *FILL_STYLE_FMT = "fill=\"{}\"";

        std::string fill_style;
        if (!do_fill) {
            fill_style = fmt::format(FILL_STYLE_FMT, "none");
        } else {
            if (state.fill_brush_.brush_type_ == NVG_BRUSH_FLAT_COLOR) {
                fill_style = fmt::format(FILL_STYLE_FMT, fmt::format("#{:02X}{:02X}{:02X}", state.fill_brush_.color_[0], state.fill_brush_.color_[1], state.fill_brush_.color_[2]));
                fill_style += fmt::format(" fill-opacity=\"{}\"", state.fill_brush_.color_[3] / 255.0f);
            } else {
                static const char *FILL_BRUSH_PREFIX = "fillBrush";
                const std::string fill_brush_name = fmt::format("{}{}", FILL_BRUSH_PREFIX, state.fill_brush_.version_);

                if (!state.fill_brush_.change_flushed_) {
                    state.fill_brush_.change_flushed_ = true;

                    if (!out.write_text(emit_complex_brush_style(state.fill_brush_, FILL_BRUSH_PREFIX))) {
                        errors.emplace_back(NVG_FAILED_TO_WRITE_TO_DEST_FILE, out.tell());
                        return false;
                    }

                    state.fill_brush_.version_++;
                }

                // NOTE: Don't add quotations on jump tag or else break
                fill_style = fmt::format(FILL_STYLE_FMT, fmt::format("url(#{})", fill_brush_name));
            }
        }

        static const char *STROKE_STYLE_FMT = "stroke=\"{}\"";
        std::string stroke_style;
        if (!do_stroke) {
            stroke_style = fmt::format(STROKE_STYLE_FMT, "none");
        } else {
            if (state.stroke_brush_.brush_type_ == NVG_BRUSH_FLAT_COLOR) {
                stroke_style = fmt::format(STROKE_STYLE_FMT, fmt::format("#{:02X}{:02X}{:02X}", state.stroke_brush_.color_[0], state.stroke_brush_.color_[1], state.stroke_brush_.color_[2]));
                stroke_style += fmt::format(" stroke-opacity=\"{}\"", state.stroke_brush_.color_[3] / 255.0f);
            } else {
                static const char *STROKE_BRUSH_PREFIX = "strokeBrush";
                const std::string stroke_brush_name = fmt::format("{}{}", STROKE_BRUSH_PREFIX, state.stroke_brush_.version_);

                if (!state.stroke_brush_.change_flushed_) {
                    state.stroke_brush_.change_flushed_ = true;

                    if (!out.write_text(emit_complex_brush_style(state.stroke_brush_, STROKE_BRUSH_PREFIX))) {
                        errors.emplace_back(NVG_FAILED_TO_WRITE_TO_DEST_FILE, out.tell());
                        return false;
                    }

                    state.stroke_brush_.version_++;
                }

                stroke_style = fmt::format(STROKE_STYLE_FMT, fmt::format("url(#{})", stroke_brush_name));
            }
        }

        std::string transform;
        if (!state.no_transform_matrix_) {
            transform = fmt::format(" transform = \"matrix({} {} {} {} {} {})\"", state.transform_matrix_[0],
                state.transform_matrix_[1], state.transform_matrix_[2], state.transform_matrix_[3], state.transform_matrix_[4],
                state.transform_matrix_[5]);
        }

        std::string stroke_width;
        if (state.stroke_width_ != 1.0f) {
            stroke_width = fmt::format(" stroke-width=\"{}\"", state.stroke_width_);
        }
        
        std::string stroke_miterlimit;
        if (state.stroke_miter_limit != 4.0f) {
            stroke_width = fmt::format(" stroke-miterlimit=\"{}\"", state.stroke_miter_limit);
        }

        if (!out.write_text(fmt::format("<path d=\"{}\" {} {}{}{}{} />\n", direction, stroke_style, fill_style, stroke_width, stroke_miterlimit, transform))) {
            errors.emplace_back(NVG_FAILED_TO_WRITE_TO_DEST_FILE, out.tell());
            return false;
        }

        return true;
    }

    bool nvg_direct_command_set_transformation(nvg_state &state, std::uint32_t command, common::ro_stream &in, common::wo_stream &out, std::vector<nvg_convert_error_description> &errors) {
        const std::uint32_t transform_type = (command >> 16) & 0xFF;
        if (transform_type == 1) {
            // Means don't use transform matrix
            state.no_transform_matrix_ = true;
        } else {
            state.no_transform_matrix_ = false;
            std::memset(state.transform_matrix_, 0, sizeof(state.transform_matrix_));

            state.transform_matrix_[0] = 1.0f;
            state.transform_matrix_[3] = 1.0f;

            // Try to put the arguments in the transform matrix, but in order that SVG uses (see mozilla page)
            // https://developer.mozilla.org/en-US/docs/Web/SVG/Attribute/transform
            // Symbian order here:
            // https://github.com/SymbianSource/oss.FCL.sf.mw.svgt/blob/a6e9f61716263f102bd673b7b9161c4519b95dae/svgtopt/nvgdecoder/src/nvg.cpp#L1498
            if ((transform_type & NVG_TRANSFORM_SET_ROTATION) || (transform_type & NVG_TRANSFORM_SET_SCALING)
                || (transform_type == NVG_TRANSFORM_SET_COMPLETE)) {
                if (in.read(&state.transform_matrix_[0], 4) != 4) {
                    errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                    return false;
                }

                if (in.read(&state.transform_matrix_[3], 4) != 4) {
                    errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                    return false;
                }
            }

            if ((transform_type & NVG_TRANSFORM_SET_ROTATION) || (transform_type & NVG_TRANSFORM_SET_SHEARING)
                || (transform_type == NVG_TRANSFORM_SET_COMPLETE)) {
                if (in.read(&state.transform_matrix_[2], 4) != 4) {
                    errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                    return false;
                }

                if (in.read(&state.transform_matrix_[1], 4) != 4) {
                    errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                    return false;
                }
            }

            if ((transform_type & NVG_TRANSFORM_SET_TRANSLATION) || (transform_type == NVG_TRANSFORM_SET_COMPLETE)) {
                if (in.read(&state.transform_matrix_[4], 4) != 4) {
                    errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                    return false;
                }

                if (in.read(&state.transform_matrix_[5], 4) != 4) {
                    errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                    return false;
                }
            }

            if ((state.transform_matrix_[0] == 1.0f) && (state.transform_matrix_[1] == 0.0f) && (state.transform_matrix_[2] == 0.0f)
                && (state.transform_matrix_[3] == 1.0f) && (state.transform_matrix_[4] == 0.0f) && (state.transform_matrix_[5] == 0.0f)) {
                state.no_transform_matrix_ = true;
            }
        }

        return true;
    }

    bool nvg_direct_command_set_stroke_width(nvg_state &state, std::uint32_t command, common::ro_stream &in, common::wo_stream &out, std::vector<nvg_convert_error_description> &errors) {
        if (in.read(&state.stroke_width_, 4) != 4) {
            errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
            return false;
        }

        return true;
    }

    bool nvg_direct_command_set_stroke_miter_limit(nvg_state &state, std::uint32_t command, common::ro_stream &in, common::wo_stream &out, std::vector<nvg_convert_error_description> &errors) {
        if (in.read(&state.stroke_miter_limit, 4) != 4) {
            errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
            return false;
        }

        return true;
    }

    static const std::map<nvg_direct_command_opcode, std::pair<std::function<bool(nvg_state &, std::uint32_t, common::ro_stream &, common::wo_stream &, std::vector<nvg_convert_error_description> &)>, bool>>
        DIRECT_COMMAND_OPCODES_HANLDER = {
            { NVG_DIRECT_COMMAND_SET_FILL_PAINT, { nvg_direct_command_set_fill_paint, true } },
            { NVG_DIRECT_COMMAND_SET_STROKE_PAINT, { nvg_direct_command_set_stroke_paint, true } },
            { NVG_DIRECT_COMMAND_SET_COLOR_RAMP, { nvg_direct_command_set_fill_color_ramp, true } },
            { NVG_DIRECT_COMMAND_DRAW_PATH, { nvg_direct_command_draw_path, true } },
            { NVG_DIRECT_COMMAND_SET_TRANSFORMATION, { nvg_direct_command_set_transformation, false } },
            { NVG_DIRECT_COMMAND_SET_STROKE_WIDTH, { nvg_direct_command_set_stroke_width, false } },
            { NVG_DIRECT_COMMAND_SET_STROKE_MITER_LIMIT, { nvg_direct_command_set_stroke_miter_limit, false } },
        };

    // Direct command structure (basic):
    // Offset vector: contains offset of the data for a command. First 2 byte is number of vector, later nvector * 2 bytes are offsets
    // Commands: Each command is 32-bit integer, upper 16-bit is opcode, lower 16-bit contains index of the data offset in the offset vector.

    bool convert_nvg_commands_to_svg(common::ro_stream &in, common::wo_stream &out, std::vector<nvg_convert_error_description> &errors, nvg_options *options) {
        std::int16_t header_size = 0;
        std::uint8_t version = 0;

        if (in.read(NVG_HEADERSIZE_OFFSET, &header_size, 2) != 2) {
            errors.emplace_back(NVG_END_OF_FILE, NVG_HEADERSIZE_OFFSET);
            return false;
        }

        if (in.read(NVG_VERSION_OFFSET, &version, 1) != 1) {
            errors.emplace_back(NVG_END_OF_FILE, NVG_VERSION_OFFSET);
            return false;
        }

        std::uint16_t path_type = 0;
        if (in.read(NVG_PATH_DATATYPE_OFFSET, &path_type, 2) != 2) {
            errors.emplace_back(NVG_END_OF_FILE, NVG_PATH_DATATYPE_OFFSET);
            return false;
        }

        float viewport[4];
        if (in.read(NVG_VIEWPORT_INFO_OFFSET, &viewport, 16) != 16) {
            errors.emplace_back(NVG_END_OF_FILE, NVG_VIEWPORT_INFO_OFFSET);
            return false;
        }

        std::uint16_t vector_count = 0;
        if (in.read(header_size, &vector_count, 2) != 2) {
            errors.emplace_back(NVG_END_OF_FILE, header_size);
            return false;
        }

        std::uint16_t vector_offset = header_size + 2;

        // 2 is the size of the vector count
        std::uint64_t commands_offset = vector_offset + 2 * vector_count;
        if (((commands_offset % 4) != 0) && (version >= 2)) {
            // Version 2 or above needs offset aligned
            commands_offset += 2;
        }

        std::string aspect_ratio_mode;
        if (options) {
            switch (options->aspect_ratio_mode_) {
            case NVG_NOT_PRESERVE_ASPECT_RATIO:
                aspect_ratio_mode = " preserveAspectRatio=\"none\"";
                break;
            case NVG_PRESERVE_ASPECT_RATIO_AND_REMOVE_UNUSED_SPACE:
                aspect_ratio_mode = " preserveAspectRatio=\"xMinYMin meet\"";
                break;
            case NVG_PRESERVE_ASPECT_RATIO_SLICE:
                aspect_ratio_mode = "preserveAspectRatio=\"xMidYMid slice\"";
                break;
            default:
                break;
            }
        }

        out.write_text(fmt::format("<svg viewBox=\"{} {} {} {}\" xmlns=\"http://www.w3.org/2000/svg\"{}{}{}>\n", viewport[0],
            viewport[1], viewport[2], viewport[3],
            (options && (options->width > 0)) ? fmt::format(" width=\"{}\"", options->width) : "",
            (options && (options->height > 0)) ? fmt::format(" height=\"{}\"", options->height) : "",
            aspect_ratio_mode));

        in.seek(commands_offset, common::seek_where::beg);

        std::uint16_t command_count = 0;
        if (in.read(&command_count, 2) != 2) {
            errors.emplace_back(NVG_END_OF_FILE, commands_offset);
            return false;
        }

        if (version >= 2) {
            in.seek(2, common::seek_where::cur);
        }

        nvg_state current_state;
        current_state.path_datatype_ = static_cast<nvg_path_data_type>(path_type);

        for (std::uint16_t i = 0; i < command_count; i++) {
            std::uint32_t command = 0;
            if (in.read(&command, 4) != 4) {
                errors.emplace_back(NVG_END_OF_FILE, in.tell());
                return false;
            }

            std::uint16_t opcode = static_cast<std::uint16_t>(command >> 24) & 0xFF;
            std::uint16_t data_offset_index_in_vector = static_cast<std::uint16_t>(command & 0xFFFF);

            std::uint64_t current = in.tell();
            std::uint16_t offset = 0;

            auto handler = DIRECT_COMMAND_OPCODES_HANLDER.find(static_cast<nvg_direct_command_opcode>(opcode));
            if (handler == DIRECT_COMMAND_OPCODES_HANLDER.end()) {
                errors.emplace_back(NVG_UNRECOGNISED_DIRECT_COMMAND_OPCODE, offset - 4, opcode);
                continue;
            }

            if (handler->second.second) {
                if (in.read(vector_offset + data_offset_index_in_vector * 2, &offset, 2) != 2) {
                    errors.emplace_back(NVG_READ_COMMAND_DATA_FAILED, in.tell());
                    continue;
                }

                in.seek(offset, common::seek_where::beg);
            }

            handler->second.first(current_state, command, in, out, errors);

            if (handler->second.second) {
                in.seek(current, common::seek_where::beg);
            }
        }

        out.write_text("</svg>");
        return true;
    }

    bool convert_nvg_to_svg(common::ro_stream &in, common::wo_stream &out, std::vector<nvg_convert_error_description> &errors, nvg_options *options) {
        // Read signature
        char signature[3];
        if (in.read(signature, 3) != 3) {
            errors.emplace_back(NVG_END_OF_FILE, 0);
            return false;
        }

        if ((signature[0] != 'n') || (signature[1] != 'v') || (signature[2] != 'g')) {
            errors.emplace_back(NVG_NOT_NVG_FILE, 0);
            return false;
        }

        std::uint16_t nvg_type = 0;
        if (in.read(NVG_TYPE_OFFSET, &nvg_type, sizeof(std::uint16_t)) != sizeof(std::uint16_t)) {
            errors.emplace_back(NVG_END_OF_FILE, NVG_TYPE_OFFSET);
            return false;
        }

        if ((nvg_type & 3) == 0) {
            return convert_nvg_commands_to_svg(in, out, errors, options);
        } else {
            errors.emplace_back(NVG_TVL_FORMAT_UNSUPPORTED, 0);
        }

        return false;
    }
}