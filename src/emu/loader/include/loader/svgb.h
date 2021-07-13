/*
 * Copyright (c) 2021 EKA2L1 Team
 *
 * Based on svgb2svg converter v.1.1.0
 * Based on svgb_decoder v.0.7.7
 * Based on svgb_dec v1.5
 *
 * This file is part of EKA2L1 project.
 * This file's original author is Ilya Averyanov, Anton Mihailov and Slava Monich
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

#pragma once

#include <common/buffer.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#define SVG_ATTR_DECODE_PROTOTYPE(f) static bool f(svgb_decode_state& state, const svg_attr * attr, const svg_element * elem, common::ro_stream& in, common::wo_stream &out);

namespace eka2l1::loader {
    struct svg_fill {
        std::string name_;
        std::uint8_t value_[4];
    };

    struct svg_path_seg_type {
        char command_;       /* command character */
        int params_;         /* number of parameters */
    };

    struct svg_element_attr {
        std::string name_;
        std::string value_;
    };

    struct svg_element {
        std::string name_;                          /* element name */
        std::int16_t code_;                         /* element code */
        const svg_element_attr* attr_;              /* additional attributes */
        int nattr_;                                 /* number of additional attributes */
    };

    struct svg_attr;
    struct svgb_decode_state;

    using svg_attr_decode_callback = std::function<bool(svgb_decode_state &, const svg_attr *, const svg_element *, common::ro_stream &in, common::wo_stream &out)>;

    struct svg_attr
    {
        std::string name_;
        svg_attr_decode_callback decode_;
    };

    enum svg_decode_elem_status {
        svg_decode_error,
        svg_decode_elem_started,
        svg_decode_elem_finished
    };

    struct svg_block {
        const svg_element * element_;
    };

    enum svgb_convert_error {
        svgb_convert_error_none,
        svgb_convert_error_trailing_date_ignored,
        svgb_convert_error_eof,
        svgb_convert_error_write_fail,
        svgb_convert_error_element_unimplemented,
        svgb_convert_error_unsupport_fill_flag,
        svgb_convert_error_unexpected_value,
        svgb_convert_error_unexpected_attribute,
        svgb_convert_error_unexpected_element,
        svgb_convert_error_cdata_ignored,
        svgb_convert_error_invalid_file
    };

    struct svgb_convert_error_description {
        svgb_convert_error reason_;
        std::uint64_t byte_position_;
        std::string data1_;
        std::string data2_;

        svgb_convert_error_description(svgb_convert_error reason, std::uint64_t byte_position)
            : reason_(reason)
            , byte_position_(byte_position) {

        }

        svgb_convert_error_description(svgb_convert_error reason, std::uint64_t byte_position, const std::string &data1)
            : reason_(reason)
            , byte_position_(byte_position)
            , data1_(data1) {

        }

        svgb_convert_error_description(svgb_convert_error reason, std::uint64_t byte_position, const std::string &data1, const std::string &data2)
            : reason_(reason)
            , byte_position_(byte_position)
            , data1_(data1)
            , data2_(data2) {

        }
    };

    bool convert_svgb_to_svg(common::ro_stream &in, common::wo_stream &out, std::vector<svgb_convert_error_description> &errors);
}
