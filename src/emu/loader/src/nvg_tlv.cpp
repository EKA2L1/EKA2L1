/*
 * Copyright (c) 2026 EKA2L1 Team
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

// NVG-TLV is a recording of OpenVG calls made by the SVG engine's pseudo renderer
// (svgt PseudoVG). Replay follows svgt's nvgdecoder CTLVRenderer, built with
// __MIRROR_: the user space is the OpenVG surface, origin bottom-left, y up.

#include <loader/nvg.h>

#include <common/buffer.h>

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace eka2l1::loader {
    namespace {
        constexpr std::uint32_t NVG_HEADER_SIZE_OFFSET = 4;
        constexpr std::uint16_t NVG_TLV_MIN_HEADER_SIZE = 8;

        enum tlv_opcode : std::uint8_t {
            TLV_CLEAR = 0,
            TLV_SETI,
            TLV_SETF,
            TLV_SETFV,
            TLV_SET_PARAMETER_I,
            TLV_SET_PARAMETER_F,
            TLV_SET_PARAMETER_FV,
            TLV_SET_COLOR,
            TLV_SET_PAINT,
            TLV_LOAD_MATRIX,
            TLV_MULT_MATRIX,
            TLV_LOAD_IDENTITY,
            TLV_SCALE,
            TLV_ROTATE,
            TLV_TRANSLATE,
            TLV_APPEND_PATH_DATA,
            TLV_DRAW_PATH,
            TLV_CLEAR_PATH,
            TLV_VGU_RECT,
            TLV_VGU_ELLIPSE,
            TLV_VGU_ROUND_RECT,
            TLV_VGU_LINE,
            TLV_CREATE_PAINT,
            TLV_CREATE_PATH,
            TLV_GET_PIXELS,
            TLV_DESTROY_IMAGE,
            TLV_DESTROY_PAINT,
            TLV_DESTROY_PATH,
            TLV_SETIV,
            TLV_CREATE_IMAGE,
            TLV_DRAW_IMAGE,
            TLV_CLEAR_IMAGE,
            TLV_IMAGE_SUB_DATA,
            TLV_PREPARE_TO_BIND_IMAGE,
            TLV_BIND_IMAGE,
            TLV_UNBIND_IMAGE,
            TLV_FLUSH
        };

        // OpenVG 1.1 enumerants (openvg.h).
        enum : std::uint32_t {
            VG_MATRIX_MODE = 0x1100,
            VG_FILL_RULE = 0x1101,
            VG_SCISSOR_RECTS = 0x1106,
            VG_STROKE_LINE_WIDTH = 0x1110,
            VG_STROKE_CAP_STYLE = 0x1111,
            VG_STROKE_JOIN_STYLE = 0x1112,
            VG_STROKE_MITER_LIMIT = 0x1113,
            VG_STROKE_DASH_PATTERN = 0x1114,
            VG_STROKE_DASH_PHASE = 0x1115,
            VG_SCISSORING = 0x1131,

            VG_MATRIX_PATH_USER_TO_SURFACE = 0x1400,
            VG_MATRIX_IMAGE_USER_TO_SURFACE = 0x1401,
            VG_MATRIX_FILL_PAINT_TO_USER = 0x1402,
            VG_MATRIX_STROKE_PAINT_TO_USER = 0x1403,

            VG_CAP_ROUND = 0x1701,
            VG_CAP_SQUARE = 0x1702,
            VG_JOIN_ROUND = 0x1801,
            VG_JOIN_BEVEL = 0x1802,
            VG_NON_ZERO = 0x1901,

            VG_PAINT_TYPE = 0x1A00,
            VG_PAINT_COLOR = 0x1A01,
            VG_PAINT_COLOR_RAMP_SPREAD_MODE = 0x1A02,
            VG_PAINT_COLOR_RAMP_STOPS = 0x1A03,
            VG_PAINT_LINEAR_GRADIENT = 0x1A04,
            VG_PAINT_RADIAL_GRADIENT = 0x1A05,

            VG_PAINT_TYPE_LINEAR_GRADIENT = 0x1B01,
            VG_PAINT_TYPE_RADIAL_GRADIENT = 0x1B02,

            VG_COLOR_RAMP_SPREAD_REPEAT = 0x1C01,
            VG_COLOR_RAMP_SPREAD_REFLECT = 0x1C02,

            VG_PATH_DATATYPE_F = 3,

            VG_STROKE_PATH = 1 << 0,
            VG_FILL_PATH = 1 << 1
        };

        enum : std::uint8_t {
            SEG_CLOSE_PATH = 0 << 1,
            SEG_MOVE_TO = 1 << 1,
            SEG_LINE_TO = 2 << 1,
            SEG_HLINE_TO = 3 << 1,
            SEG_VLINE_TO = 4 << 1,
            SEG_QUAD_TO = 5 << 1,
            SEG_CUBIC_TO = 6 << 1,
            SEG_SQUAD_TO = 7 << 1,
            SEG_SCUBIC_TO = 8 << 1,
            SEG_SCCWARC_TO = 9 << 1,
            SEG_SCWARC_TO = 10 << 1,
            SEG_LCCWARC_TO = 11 << 1,
            SEG_LCWARC_TO = 12 << 1,
            SEG_RELATIVE = 1
        };

        // Column-major, as OpenVG stores it: { sx, shy, w0, shx, sy, w1, tx, ty, w2 }.
        using vg_matrix = std::array<float, 9>;

        constexpr vg_matrix IDENTITY_MATRIX = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };

        vg_matrix multiply(const vg_matrix &a, const vg_matrix &b) {
            vg_matrix r{};
            for (int col = 0; col < 3; col++) {
                for (int row = 0; row < 3; row++) {
                    r[col * 3 + row] = a[0 * 3 + row] * b[col * 3 + 0] + a[1 * 3 + row] * b[col * 3 + 1]
                        + a[2 * 3 + row] * b[col * 3 + 2];
                }
            }
            return r;
        }

        std::string svg_matrix(const vg_matrix &m) {
            return fmt::format("matrix({} {} {} {} {} {})", m[0], m[1], m[3], m[4], m[6], m[7]);
        }

        struct tlv_paint {
            std::uint32_t type_ = 0x1B00;
            float color_[4] = { 0, 0, 0, 1 };
            std::uint32_t spread_ = 0x1C00;
            std::vector<float> stops_;
            float linear_[4] = { 0, 0, 1, 0 };
            float radial_[5] = { 0, 0, 0, 0, 1 };
        };

        struct tlv_reader {
            common::ro_stream &in_;
            bool failed_ = false;

            template <typename T>
            T read() {
                T value{};
                if (!failed_ && (in_.read(&value, sizeof(T)) != sizeof(T))) {
                    failed_ = true;
                }
                return value;
            }

            float read_f32() {
                return read<float>();
            }

            bool skip(const std::uint64_t amount) {
                if (failed_ || (in_.left() < amount)) {
                    failed_ = true;
                    return false;
                }

                in_.seek(static_cast<std::int64_t>(amount), common::seek_where::cur);
                return true;
            }
        };

        class tlv_renderer {
            tlv_reader reader_;
            common::wo_stream &out_;
            std::vector<nvg_convert_error_description> &errors_;

            int target_width_;
            int target_height_;

            std::map<std::uint32_t, vg_matrix> matrices_;
            std::uint32_t matrix_mode_ = VG_MATRIX_PATH_USER_TO_SURFACE;
            vg_matrix user_matrix_ = IDENTITY_MATRIX;
            bool scaled_ = false;

            std::map<std::int32_t, tlv_paint> paints_;
            tlv_paint fill_paint_;
            tlv_paint stroke_paint_;

            std::uint32_t fill_rule_ = 0x1900;
            float stroke_width_ = 1.0f;
            std::uint32_t cap_style_ = 0x1700;
            std::uint32_t join_style_ = 0x1800;
            float miter_limit_ = 4.0f;
            std::vector<float> dash_pattern_;
            float dash_phase_ = 0.0f;

            bool scissoring_ = false;
            std::vector<std::int32_t> scissor_rects_;
            std::string scissor_clip_id_;

            std::uint32_t path_datatype_ = VG_PATH_DATATYPE_F;
            float path_scale_ = 1.0f;
            float path_bias_ = 0.0f;
            std::string path_;

            int next_def_id_ = 0;

            vg_matrix &current_matrix() {
                return matrices_[matrix_mode_];
            }

            void append_segments(const std::vector<std::uint8_t> &segments, const std::vector<float> &coords);
            std::string emit_paint(const char *prefix, const tlv_paint &paint, const vg_matrix &paint_to_user);
            void draw_path(const std::uint32_t paint_modes);
            bool execute(const std::uint8_t opcode);

        public:
            explicit tlv_renderer(common::ro_stream &in, common::wo_stream &out,
                std::vector<nvg_convert_error_description> &errors, nvg_options *options)
                : reader_{ in }
                , out_(out)
                , errors_(errors)
                , target_width_((options && (options->width > 0)) ? options->width : -1)
                , target_height_((options && (options->height > 0)) ? options->height : -1) {
                for (const std::uint32_t mode : { VG_MATRIX_PATH_USER_TO_SURFACE, VG_MATRIX_IMAGE_USER_TO_SURFACE,
                         VG_MATRIX_FILL_PAINT_TO_USER, VG_MATRIX_STROKE_PAINT_TO_USER }) {
                    matrices_[mode] = IDENTITY_MATRIX;
                }
            }

            void run(const std::uint64_t commands_offset);

            int width() const {
                return target_width_;
            }

            int height() const {
                return target_height_;
            }
        };

        void tlv_renderer::append_segments(const std::vector<std::uint8_t> &segments, const std::vector<float> &coords) {
            static const std::map<std::uint8_t, std::pair<char, int>> SEGMENT_INFO = {
                { SEG_MOVE_TO, { 'M', 2 } },
                { SEG_LINE_TO, { 'L', 2 } },
                { SEG_HLINE_TO, { 'H', 1 } },
                { SEG_VLINE_TO, { 'V', 1 } },
                { SEG_QUAD_TO, { 'Q', 4 } },
                { SEG_CUBIC_TO, { 'C', 6 } },
                { SEG_SQUAD_TO, { 'T', 2 } },
                { SEG_SCUBIC_TO, { 'S', 4 } },
                { SEG_SCCWARC_TO, { 'A', 5 } },
                { SEG_SCWARC_TO, { 'A', 5 } },
                { SEG_LCCWARC_TO, { 'A', 5 } },
                { SEG_LCWARC_TO, { 'A', 5 } }
            };

            std::size_t ci = 0;

            for (const std::uint8_t segment : segments) {
                const std::uint8_t command = segment & ~SEG_RELATIVE;

                if (command == SEG_CLOSE_PATH) {
                    path_ += "Z ";
                    continue;
                }

                const auto info = SEGMENT_INFO.find(command);
                if (info == SEGMENT_INFO.end()) {
                    errors_.emplace_back(NVG_UNKNOWN_PATH_SEGMENT_TYPE, reader_.in_.tell(), command);
                    return;
                }

                const int count = info->second.second;
                if (ci + count > coords.size()) {
                    errors_.emplace_back(NVG_READ_COMMAND_DATA_FAILED, reader_.in_.tell());
                    return;
                }

                // An OpenVG path starts at the origin; SVG wants an explicit move first.
                if (path_.empty() && (command != SEG_MOVE_TO)) {
                    path_ += "M 0 0 ";
                }

                const char letter = (segment & SEG_RELATIVE) ? static_cast<char>(std::tolower(info->second.first))
                                                             : info->second.first;
                const float *v = coords.data() + ci;
                ci += count;

                if (letter == 'A' || letter == 'a') {
                    // In the y-up user space a counter-clockwise arc runs in the positive angle direction.
                    const bool ccw = (command == SEG_SCCWARC_TO) || (command == SEG_LCCWARC_TO);
                    const bool large = (command == SEG_LCCWARC_TO) || (command == SEG_LCWARC_TO);

                    path_ += fmt::format("{} {} {} {} {} {} {} {} ", letter, std::fabs(v[0]), std::fabs(v[1]), v[2],
                        large ? 1 : 0, ccw ? 1 : 0, v[3], v[4]);
                    continue;
                }

                path_ += letter;
                for (int i = 0; i < count; i++) {
                    path_ += fmt::format(" {}", v[i]);
                }
                path_ += ' ';
            }
        }

        std::string svg_color(const float r, const float g, const float b) {
            const auto channel = [](const float v) {
                return static_cast<int>(std::round(std::clamp(v, 0.0f, 1.0f) * 255.0f));
            };

            return fmt::format("#{:02X}{:02X}{:02X}", channel(r), channel(g), channel(b));
        }

        std::string tlv_renderer::emit_paint(const char *prefix, const tlv_paint &paint, const vg_matrix &paint_to_user) {
            const bool linear = (paint.type_ == VG_PAINT_TYPE_LINEAR_GRADIENT);
            const bool radial = (paint.type_ == VG_PAINT_TYPE_RADIAL_GRADIENT);

            if (!linear && !radial) {
                return fmt::format(" {0}=\"{1}\" {0}-opacity=\"{2}\"", prefix,
                    svg_color(paint.color_[0], paint.color_[1], paint.color_[2]), std::clamp(paint.color_[3], 0.0f, 1.0f));
            }

            const std::string id = fmt::format("tlvpaint{}", next_def_id_++);
            const char *spread = (paint.spread_ == VG_COLOR_RAMP_SPREAD_REPEAT) ? "repeat"
                : (paint.spread_ == VG_COLOR_RAMP_SPREAD_REFLECT)                ? "reflect"
                                                                                 : "pad";

            std::string def;
            if (linear) {
                def = fmt::format("<linearGradient id=\"{}\" x1=\"{}\" y1=\"{}\" x2=\"{}\" y2=\"{}\"", id,
                    paint.linear_[0], paint.linear_[1], paint.linear_[2], paint.linear_[3]);
            } else {
                def = fmt::format("<radialGradient id=\"{}\" cx=\"{}\" cy=\"{}\" fx=\"{}\" fy=\"{}\" r=\"{}\"", id,
                    paint.radial_[0], paint.radial_[1], paint.radial_[2], paint.radial_[3], paint.radial_[4]);
            }

            def += fmt::format(" gradientUnits=\"userSpaceOnUse\" spreadMethod=\"{}\" gradientTransform=\"{}\">", spread,
                svg_matrix(paint_to_user));

            for (std::size_t i = 0; i + 5 <= paint.stops_.size(); i += 5) {
                const float *stop = paint.stops_.data() + i;
                def += fmt::format("<stop offset=\"{}\" stop-color=\"{}\" stop-opacity=\"{}\"/>", std::clamp(stop[0], 0.0f, 1.0f),
                    svg_color(stop[1], stop[2], stop[3]), std::clamp(stop[4], 0.0f, 1.0f));
            }

            def += linear ? "</linearGradient>" : "</radialGradient>";
            out_.write_text("<defs>" + def + "</defs>\n");

            return fmt::format(" {}=\"url(#{})\"", prefix, id);
        }

        void tlv_renderer::draw_path(const std::uint32_t paint_modes) {
            if (path_.empty() || !(paint_modes & (VG_FILL_PATH | VG_STROKE_PATH))) {
                return;
            }

            if (scissoring_ && scissor_rects_.size() < 4) {
                return;
            }

            std::string attributes;

            if (paint_modes & VG_FILL_PATH) {
                attributes += emit_paint("fill", fill_paint_, matrices_[VG_MATRIX_FILL_PAINT_TO_USER]);
                attributes += fmt::format(" fill-rule=\"{}\"", (fill_rule_ == VG_NON_ZERO) ? "nonzero" : "evenodd");
            } else {
                attributes += " fill=\"none\"";
            }

            if (paint_modes & VG_STROKE_PATH) {
                attributes += emit_paint("stroke", stroke_paint_, matrices_[VG_MATRIX_STROKE_PAINT_TO_USER]);
                attributes += fmt::format(" stroke-width=\"{}\" stroke-miterlimit=\"{}\"", stroke_width_, std::max(1.0f, miter_limit_));
                attributes += fmt::format(" stroke-linecap=\"{}\"", (cap_style_ == VG_CAP_ROUND) ? "round"
                        : (cap_style_ == VG_CAP_SQUARE)                                         ? "square"
                                                                                                : "butt");
                attributes += fmt::format(" stroke-linejoin=\"{}\"", (join_style_ == VG_JOIN_ROUND) ? "round"
                        : (join_style_ == VG_JOIN_BEVEL)                                          ? "bevel"
                                                                                                  : "miter");

                if (dash_pattern_.size() >= 2) {
                    std::string dashes;
                    for (std::size_t i = 0; i + 1 < dash_pattern_.size(); i += 2) {
                        dashes += fmt::format("{}{} {}", dashes.empty() ? "" : " ", std::max(0.0f, dash_pattern_[i]),
                            std::max(0.0f, dash_pattern_[i + 1]));
                    }

                    attributes += fmt::format(" stroke-dasharray=\"{}\" stroke-dashoffset=\"{}\"", dashes, dash_phase_);
                }
            }

            if (scissoring_ && scissor_clip_id_.empty()) {
                scissor_clip_id_ = fmt::format("tlvclip{}", next_def_id_++);

                std::string clip = fmt::format("<defs><clipPath id=\"{}\">", scissor_clip_id_);
                for (std::size_t i = 0; i + 4 <= scissor_rects_.size(); i += 4) {
                    clip += fmt::format("<rect x=\"{}\" y=\"{}\" width=\"{}\" height=\"{}\"/>", scissor_rects_[i],
                        scissor_rects_[i + 1], scissor_rects_[i + 2], scissor_rects_[i + 3]);
                }
                clip += "</clipPath></defs>\n";
                out_.write_text(clip);
            }

            const std::string element = fmt::format("<path d=\"{}\" transform=\"{}\"{}/>", path_,
                svg_matrix(matrices_[VG_MATRIX_PATH_USER_TO_SURFACE]), attributes);

            if (scissoring_) {
                out_.write_text(fmt::format("<g clip-path=\"url(#{})\">{}</g>\n", scissor_clip_id_, element));
            } else {
                out_.write_text(element + "\n");
            }
        }

        bool tlv_renderer::execute(const std::uint8_t opcode) {
            tlv_reader &r = reader_;

            switch (opcode) {
            case TLV_CLEAR: {
                r.read_f32();
                r.read_f32();
                const float width = r.read_f32();
                const float height = r.read_f32();

                // The renderer never fills the clear colour; the first clear only fits the
                // recorded canvas to the target, without preserving aspect ratio.
                if (!scaled_ && (width > 0.0f) && (height > 0.0f)) {
                    if (target_width_ <= 0) {
                        target_width_ = static_cast<int>(width);
                    }
                    if (target_height_ <= 0) {
                        target_height_ = static_cast<int>(height);
                    }

                    vg_matrix m = user_matrix_;
                    m = multiply(m, { 1, 0, 0, 0, 1, 0, target_width_ / 2.0f, target_height_ / 2.0f, 1 });
                    m = multiply(m, { target_width_ / width, 0, 0, 0, target_height_ / height, 0, 0, 0, 1 });
                    m = multiply(m, { 1, 0, 0, 0, 1, 0, -width / 2.0f, -height / 2.0f, 1 });

                    current_matrix() = m;
                    user_matrix_ = m;
                    scaled_ = true;
                }
                break;
            }

            case TLV_SETI: {
                const std::uint16_t type = r.read<std::uint16_t>();
                const std::uint16_t value = r.read<std::uint16_t>();

                switch (type) {
                case VG_MATRIX_MODE:
                    if (matrices_.count(value)) {
                        matrix_mode_ = value;
                    }
                    break;
                case VG_FILL_RULE:
                    fill_rule_ = value;
                    break;
                case VG_STROKE_CAP_STYLE:
                    cap_style_ = value;
                    break;
                case VG_STROKE_JOIN_STYLE:
                    join_style_ = value;
                    break;
                case VG_SCISSORING:
                    scissoring_ = (value != 0);
                    break;
                default:
                    break;
                }
                break;
            }

            case TLV_SETF: {
                const std::uint16_t type = r.read<std::uint16_t>();
                const float value = r.read_f32();

                if (type == VG_STROKE_LINE_WIDTH) {
                    stroke_width_ = value;
                } else if (type == VG_STROKE_MITER_LIMIT) {
                    miter_limit_ = value;
                } else if (type == VG_STROKE_DASH_PHASE) {
                    dash_phase_ = value;
                }
                break;
            }

            case TLV_SETFV: {
                const std::uint16_t type = r.read<std::uint16_t>();
                const std::uint16_t count = r.read<std::uint16_t>();

                std::vector<float> values(count);
                for (float &value : values) {
                    value = r.read_f32();
                }

                if (type == VG_STROKE_DASH_PATTERN) {
                    dash_pattern_ = std::move(values);
                }
                break;
            }

            case TLV_SETIV: {
                const std::uint16_t type = r.read<std::uint16_t>();
                const std::uint16_t count = r.read<std::uint16_t>();

                std::vector<std::int32_t> values(count);
                for (std::int32_t &value : values) {
                    value = r.read<std::int32_t>();
                }

                if (type == VG_SCISSOR_RECTS) {
                    // Scissor rectangles are given in user space and bounded in surface space.
                    const vg_matrix &m = user_matrix_;
                    for (std::size_t i = 0; i + 4 <= values.size(); i += 4) {
                        float min_x = 0, min_y = 0, max_x = 0, max_y = 0;

                        for (int corner = 0; corner < 4; corner++) {
                            const float x = static_cast<float>(values[i] + ((corner & 1) ? values[i + 2] : 0));
                            const float y = static_cast<float>(values[i + 1] + ((corner & 2) ? values[i + 3] : 0));
                            const float tx = static_cast<float>(static_cast<int>(m[0] * x + m[3] * y + m[6]));
                            const float ty = static_cast<float>(static_cast<int>(m[1] * x + m[4] * y + m[7]));

                            min_x = corner ? std::min(min_x, tx) : tx;
                            min_y = corner ? std::min(min_y, ty) : ty;
                            max_x = corner ? std::max(max_x, tx) : tx;
                            max_y = corner ? std::max(max_y, ty) : ty;
                        }

                        values[i] = static_cast<std::int32_t>(min_x);
                        values[i + 1] = static_cast<std::int32_t>(min_y);
                        values[i + 2] = static_cast<std::int32_t>(max_x - min_x);
                        values[i + 3] = static_cast<std::int32_t>(max_y - min_y);
                    }

                    scissor_rects_ = std::move(values);
                    scissor_clip_id_.clear();
                }
                break;
            }

            case TLV_SET_PARAMETER_I: {
                const std::int32_t handle = r.read<std::int32_t>();
                const std::uint16_t type = r.read<std::uint16_t>();
                const std::uint16_t value = r.read<std::uint16_t>();

                tlv_paint &paint = paints_[handle];
                if (type == VG_PAINT_TYPE) {
                    paint.type_ = value;
                } else if (type == VG_PAINT_COLOR_RAMP_SPREAD_MODE) {
                    paint.spread_ = value;
                }
                break;
            }

            case TLV_SET_PARAMETER_F:
                r.read<std::int32_t>();
                r.read<std::uint16_t>();
                r.read_f32();
                break;

            case TLV_SET_PARAMETER_FV: {
                const std::int32_t handle = r.read<std::int32_t>();
                const std::uint16_t type = r.read<std::uint16_t>();
                const std::uint32_t count = r.read<std::uint32_t>();

                if (count > r.in_.left() / sizeof(float)) {
                    r.failed_ = true;
                    break;
                }

                std::vector<float> values(count);
                for (float &value : values) {
                    value = r.read_f32();
                }

                tlv_paint &paint = paints_[handle];
                if ((type == VG_PAINT_COLOR) && (count >= 4)) {
                    std::copy(values.begin(), values.begin() + 4, paint.color_);
                } else if (type == VG_PAINT_COLOR_RAMP_STOPS) {
                    paint.stops_ = std::move(values);
                } else if ((type == VG_PAINT_LINEAR_GRADIENT) && (count >= 4)) {
                    std::copy(values.begin(), values.begin() + 4, paint.linear_);
                } else if ((type == VG_PAINT_RADIAL_GRADIENT) && (count >= 5)) {
                    std::copy(values.begin(), values.begin() + 5, paint.radial_);
                }
                break;
            }

            case TLV_SET_COLOR: {
                const std::int32_t handle = r.read<std::int32_t>();
                const std::uint32_t rgba = r.read<std::uint32_t>();

                tlv_paint &paint = paints_[handle];
                for (int i = 0; i < 4; i++) {
                    paint.color_[i] = static_cast<float>((rgba >> (24 - i * 8)) & 0xFF) / 255.0f;
                }
                break;
            }

            case TLV_SET_PAINT: {
                const std::int32_t handle = r.read<std::int32_t>();
                const std::uint8_t modes = r.read<std::uint8_t>();

                const auto paint = paints_.find(handle);
                const tlv_paint bound = (paint != paints_.end()) ? paint->second : tlv_paint{};

                if (modes & VG_FILL_PATH) {
                    fill_paint_ = bound;
                }
                if (modes & VG_STROKE_PATH) {
                    stroke_paint_ = bound;
                }
                break;
            }

            case TLV_LOAD_MATRIX:
            case TLV_MULT_MATRIX: {
                vg_matrix m;
                for (float &value : m) {
                    value = r.read_f32();
                }

                if (opcode == TLV_MULT_MATRIX) {
                    current_matrix() = multiply(current_matrix(), m);
                } else if ((matrix_mode_ == VG_MATRIX_PATH_USER_TO_SURFACE) || (matrix_mode_ == VG_MATRIX_IMAGE_USER_TO_SURFACE)) {
                    // Loaded matrices stay relative to the caller's (fitted) matrix.
                    current_matrix() = multiply(user_matrix_, m);
                } else {
                    current_matrix() = m;
                }
                break;
            }

            case TLV_LOAD_IDENTITY:
                if (matrix_mode_ == VG_MATRIX_IMAGE_USER_TO_SURFACE) {
                    current_matrix() = IDENTITY_MATRIX;
                }
                break;

            case TLV_SCALE: {
                const float sx = r.read_f32();
                const float sy = r.read_f32();
                current_matrix() = multiply(current_matrix(), { sx, 0, 0, 0, sy, 0, 0, 0, 1 });
                break;
            }

            case TLV_TRANSLATE: {
                const float tx = r.read_f32();
                const float ty = r.read_f32();
                current_matrix() = multiply(current_matrix(), { 1, 0, 0, 0, 1, 0, tx, ty, 1 });
                break;
            }

            case TLV_ROTATE: {
                const float radians = r.read_f32() * 3.14159265358979f / 180.0f;
                const float c = std::cos(radians);
                const float s = std::sin(radians);
                current_matrix() = multiply(current_matrix(), { c, s, 0, -s, c, 0, 0, 0, 1 });
                break;
            }

            case TLV_CREATE_PATH: {
                r.read<std::uint32_t>();
                path_datatype_ = r.read<std::uint8_t>();
                path_scale_ = r.read_f32();
                path_bias_ = r.read_f32();
                r.skip(4 * 4);
                path_.clear();
                break;
            }

            case TLV_APPEND_PATH_DATA: {
                r.read<std::uint32_t>();
                const std::uint16_t segment_count = r.read<std::uint16_t>();

                std::vector<std::uint8_t> segments(segment_count);
                for (std::uint8_t &segment : segments) {
                    segment = r.read<std::uint8_t>();
                }

                // Every coordinate is recorded as 32 bits whatever the path datatype says.
                const std::uint16_t coord_count = r.read<std::uint16_t>();
                std::vector<float> coords(coord_count);

                for (float &coord : coords) {
                    const std::uint32_t raw = r.read<std::uint32_t>();
                    float value = 0.0f;

                    if (path_datatype_ == VG_PATH_DATATYPE_F) {
                        std::memcpy(&value, &raw, sizeof(float));
                    } else {
                        value = static_cast<float>(static_cast<std::int32_t>(raw));
                    }

                    coord = value * path_scale_ + path_bias_;
                }

                if (!r.failed_) {
                    append_segments(segments, coords);
                }
                break;
            }

            case TLV_DRAW_PATH: {
                r.read<std::uint32_t>();
                const std::uint16_t paint_modes = r.read<std::uint16_t>();

                if (!r.failed_) {
                    draw_path(paint_modes);
                }
                path_.clear();
                break;
            }

            case TLV_CLEAR_PATH:
                r.read<std::uint32_t>();
                r.read<std::uint16_t>();
                path_.clear();
                break;

            case TLV_VGU_RECT:
            case TLV_VGU_ELLIPSE:
            case TLV_VGU_LINE: {
                r.read<std::uint32_t>();
                float v[4];
                for (float &value : v) {
                    value = r.read_f32();
                }

                if (r.failed_) {
                    break;
                }

                if (opcode == TLV_VGU_RECT) {
                    append_segments({ SEG_MOVE_TO, SEG_HLINE_TO | SEG_RELATIVE, SEG_VLINE_TO | SEG_RELATIVE,
                                        SEG_HLINE_TO | SEG_RELATIVE, SEG_CLOSE_PATH },
                        { v[0], v[1], v[2], v[3], -v[2] });
                } else if (opcode == TLV_VGU_ELLIPSE) {
                    const float rx = v[2] / 2;
                    const float ry = v[3] / 2;
                    append_segments({ SEG_MOVE_TO, SEG_SCCWARC_TO | SEG_RELATIVE, SEG_SCCWARC_TO | SEG_RELATIVE, SEG_CLOSE_PATH },
                        { v[0] + rx, v[1], rx, ry, 0, -v[2], 0, rx, ry, 0, v[2], 0 });
                } else {
                    append_segments({ SEG_MOVE_TO, SEG_LINE_TO }, { v[0], v[1], v[2], v[3] });
                }
                break;
            }

            case TLV_VGU_ROUND_RECT: {
                r.read<std::uint32_t>();
                const float x = r.read_f32();
                const float y = r.read_f32();
                const float width = r.read_f32();
                const float height = r.read_f32();
                const float arc_width = std::clamp(r.read_f32(), 0.0f, std::max(width, 0.0f));
                const float arc_height = std::clamp(r.read_f32(), 0.0f, std::max(height, 0.0f));

                if (r.failed_) {
                    break;
                }

                const float aw = arc_width / 2;
                const float ah = arc_height / 2;

                append_segments({ SEG_MOVE_TO, SEG_HLINE_TO | SEG_RELATIVE, SEG_SCCWARC_TO | SEG_RELATIVE,
                                    SEG_VLINE_TO | SEG_RELATIVE, SEG_SCCWARC_TO | SEG_RELATIVE, SEG_HLINE_TO | SEG_RELATIVE,
                                    SEG_SCCWARC_TO | SEG_RELATIVE, SEG_VLINE_TO | SEG_RELATIVE, SEG_SCCWARC_TO | SEG_RELATIVE,
                                    SEG_CLOSE_PATH },
                    { x + aw, y, width - arc_width, aw, ah, 0, aw, ah, height - arc_height, aw, ah, 0, -aw, ah,
                        -(width - arc_width), aw, ah, 0, -aw, -ah, -(height - arc_height), aw, ah, 0, aw, -ah });
                break;
            }

            case TLV_CREATE_PAINT:
                paints_[r.read<std::int32_t>()] = tlv_paint{};
                break;

            case TLV_DESTROY_PAINT:
                paints_.erase(r.read<std::int32_t>());
                break;

            case TLV_DESTROY_IMAGE:
            case TLV_DRAW_IMAGE:
            case TLV_BIND_IMAGE:
                r.read<std::int32_t>();
                break;

            // Raster images are skipped: the SVG engine only records them for <image>.
            case TLV_CREATE_IMAGE:
                r.skip(4 * 3 + 1 + 4);
                break;

            case TLV_CLEAR_IMAGE:
                r.skip(4 * 5);
                break;

            case TLV_IMAGE_SUB_DATA: {
                r.skip(4 * 7);
                const std::uint32_t length = r.read<std::uint32_t>();
                r.skip(length);
                break;
            }

            case TLV_GET_PIXELS:
            case TLV_DESTROY_PATH:
            case TLV_PREPARE_TO_BIND_IMAGE:
            case TLV_UNBIND_IMAGE:
            case TLV_FLUSH:
                break;

            default:
                errors_.emplace_back(NVG_UNRECOGNISED_DIRECT_COMMAND_OPCODE, reader_.in_.tell() - 1, opcode);
                return false;
            }

            return !r.failed_;
        }

        void tlv_renderer::run(const std::uint64_t commands_offset) {
            reader_.in_.seek(static_cast<std::int64_t>(commands_offset), common::seek_where::beg);

            std::uint8_t opcode = 0;
            while (reader_.in_.read(&opcode, 1) == 1) {
                // Like the native renderer, stop at the first bad command and keep what was drawn.
                if (!execute(opcode)) {
                    break;
                }
            }
        }
    }

    bool convert_nvg_tlv_to_svg(common::ro_stream &in, common::wo_stream &out, std::vector<nvg_convert_error_description> &errors,
        nvg_options *options) {
        std::uint16_t header_size = 0;
        if ((in.read(NVG_HEADER_SIZE_OFFSET, &header_size, sizeof(header_size)) != sizeof(header_size)) || (header_size < NVG_TLV_MIN_HEADER_SIZE)) {
            errors.emplace_back(NVG_END_OF_FILE, NVG_HEADER_SIZE_OFFSET);
            return false;
        }

        // The canvas size is only settled by the first clear, so the body is replayed before the root is written.
        common::wo_growable_buf_stream body;
        tlv_renderer renderer(in, body, errors, options);
        renderer.run(header_size);

        const int width = renderer.width();
        const int height = renderer.height();

        if ((width <= 0) || (height <= 0)) {
            errors.emplace_back(NVG_END_OF_FILE, in.tell());
            return false;
        }

        // Flip the y-up OpenVG surface into SVG's y-down space.
        return out.write_text(fmt::format("<svg viewBox=\"0 0 {0} {1}\" width=\"{0}\" height=\"{1}\" xmlns=\"http://www.w3.org/2000/svg\">\n"
                                          "<g transform=\"matrix(1 0 0 -1 0 {1})\">\n",
                   width, height))
            && out.write_text(body.content()) && out.write_text("</g>\n</svg>");
    }
}
