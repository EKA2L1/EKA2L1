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

#include <loader/svgb.h>
#include <fmt/format.h>
#include <common/cvt.h>

#include <stack>
#include <optional>

namespace eka2l1::loader {
    static const std::int32_t SVG_FILE_START = 0x03FA56CC;
    static const std::int32_t SVG_FILE_START2 = 0x03FA56CD;
    static const std::int32_t SVG_FILE_START3 = 0x03FA56CE;
    static const std::int32_t SVG_FILE_START4 = 0x03FA56CF;
    static const std::uint8_t  SVG_FILE_END = 0xFF;
    static const std::uint8_t  SVG_ATR_END[2] = { 0xE8, 0x03 };
    static const std::uint8_t  SVG_ELEMENT_END = 0xFE;
    static const std::uint8_t  SVG_CDATA = 0xFD;

    static const svg_fill SVGB_FILL[] =
    {
        { "none",           {0xFF, 0xFF, 0xFF, 0x01}},
        { "currentColor",   {0xFF, 0xFF, 0xFF, 0x02}}
    };

    static const svg_path_seg_type SVG_PATH_SEG [] =
    {
        {'M', 2},   /* moveto */
        {'L', 2},   /* lineto */
        {'Q', 4},   /* quadratic curve */
        {'C', 6},   /* cubic curve */
        {'z', 0}    /* closepath */
    };

    struct svgb_decode_state {
        int is_new_ = 0;
        int wrap_level_ = 0;

        std::vector<svgb_convert_error_description> &error_list_;

        explicit svgb_decode_state(std::vector<svgb_convert_error_description> &descs)
            : error_list_(descs) {

        }

        void wrap_indent(int n) {
            wrap_level_ += n;
        }

        void add_error(common::ro_stream &in, svgb_convert_error reason) {
            error_list_.push_back({ reason, in.tell() });
        }

        void add_error(common::ro_stream &in, svgb_convert_error reason, const std::string &data1) {
            error_list_.push_back({ reason, in.tell(), data1 });
        }

        void add_error(common::ro_stream &in, svgb_convert_error reason, const std::string &data1, const std::string &data2) {
            error_list_.push_back({ reason, in.tell(), data1, data2 });
        }
    };

    static const std::string SVG_START = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                                     "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.0//EN\" "
                                     "\"http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd\">\n";

    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_fail);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_size);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_visibility);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_float);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_opacity);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_rect);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_string);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_grad_units);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_display);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_spread_method);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_font_style);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_font_weight);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_text_anchor);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_text_decor);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_trans);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_grad_trans);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_points);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_path);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_type);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_fill);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_css_color);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_href);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_lhref);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_dash_array);
    SVG_ATTR_DECODE_PROTOTYPE(svgb_decode_un);

    static const svg_attr SVGB_ATTRS [] =
    {

        {"fill", svgb_decode_fill },                     /* 0    00 */
        {"stroke", svgb_decode_css_color },               /* 1    01 */
        {"stroke-width", svgb_decode_float },            /* 2    02 */
        {"visibility", svgb_decode_visibility },         /* 3    03 */
        {"font-family", svgb_decode_string },            /* 4    04 */
        {"font-size", svgb_decode_float },               /* 5    05 */
        {"font-style", svgb_decode_font_style },          /* 6    06 */
        {"font-weight", svgb_decode_font_weight },        /* 7    07 */
        {"stroke-dasharray", svgb_decode_dash_array },    /* 8    08 */
        {"display", svgb_decode_display },               /* 9    09 */
        {"fill-rule", svgb_decode_string },              /* 10   0A */
        {"stroke-linecap", svgb_decode_string },         /* 11   0B */
        {"stroke-linejoin", svgb_decode_string },        /* 12   0C */
        {"stroke-dashoffset", svgb_decode_float },       /* 13   0D */
        {"stroke-miterlimit", svgb_decode_float },       /* 14   0E */
        {"color", svgb_decode_css_color },                /* 15   0F */
        {"text-anchor", svgb_decode_text_anchor },        /* 16   10 */
        {"text-decoration", svgb_decode_text_decor },     /* 17   11 */
        {"color-interpolation", svgb_decode_fail },      /* 18   12 */
        {"color-rendering", svgb_decode_fail },          /* 19   13 */
        {"letterSpacing", svgb_decode_fail },            /* 20   14 */
        {"word-spacing", svgb_decode_fail },             /* 21   15 */
        {"opacity", svgb_decode_opacity },               /* 22   16 */
        {"stroke-opacity", svgb_decode_float },          /* 23   17 */
        {"fill-opacity", svgb_decode_opacity },   /* 24   18 */
        //{"opacity", svgb_decode_opacity },               /* 24   18 */

        //{"font", svgb_decode_fail },                   /* 25   19 */
        {"textLength", svgb_decode_fail },               /* 25   19 */

        {"width", svgb_decode_size },                    /* 26   1A */
        {"height", svgb_decode_size },                   /* 27   1B */
        {"r", svgb_decode_float },                       /* 28   1C */
        {"rx", svgb_decode_float },                      /* 29   1D */

        {"ry", svgb_decode_float },                      /* 30   1E */
        {"horizAdvX", svgb_decode_fail },                /* 31   1F  */
        {"horizOriginX", svgb_decode_fail },             /* 32   20 */
        {"horizOriginY", svgb_decode_fail },             /* 33   21 */
        {"ascent", svgb_decode_fail },                   /* 34   22 */
        {"descent", svgb_decode_fail },                  /* 35   23 */
        {"alphabetic", svgb_decode_fail },               /* 36   24 */
        {"underlinePosition", svgb_decode_fail },        /* 37   25 */
        {"underlineThickness", svgb_decode_fail },       /* 38   26 */
        {"overlinePosition", svgb_decode_fail },         /* 39   27 */

        {"overlineThickness", svgb_decode_fail },        /* 40   28 */
        {"strikethroughPosition", svgb_decode_fail },    /* 41   29 */
        {"strikethroughThickness", svgb_decode_fail },   /* 42   2A */
        {"unitsPerEm", svgb_decode_fail },               /* 43   2B */
        {"wordSpacing", svgb_decode_fail },              /* 44   2C */
        {"letterSpacing", svgb_decode_fail },            /* 45   2D */
        {"cx", svgb_decode_float },                      /* 46   2E */
        {"cy", svgb_decode_float },                      /* 47   2F */
        {"y", svgb_decode_float },                       /* 48   30 */
        {"x", svgb_decode_float },                       /* 49   31 */

        {"y1", svgb_decode_float },                      /* 50   32 */
        {"y2", svgb_decode_float },                      /* 51   33 */
        {"x1", svgb_decode_float },                      /* 52   34 */
        {"x2", svgb_decode_float },                      /* 53   35 */
        {"k", svgb_decode_fail },                        /* 54   36 */
        {"g1", svgb_decode_fail },                       /* 55   37 */
        {"g2", svgb_decode_fail },                       /* 56   38 */
        {"u1", svgb_decode_fail },                       /* 57   39 */
        {"u2", svgb_decode_fail },                       /* 58   3A */
        {"unicode", svgb_decode_fail },                  /* 59   3B */

        {"glyphName", svgb_decode_fail },                /* 60   3C */
        {"lang", svgb_decode_fail },                     /* 61   3D */
        {"textDecoration", svgb_decode_fail },           /* 62   3E */
        {"textAnchor", svgb_decode_fail },               /* 63   3F */
        {"rotate", svgb_decode_fail },                   /* 64   40 */
        {"cdata", svgb_decode_fail },                    /* 65   41 */
        {"transform", svgb_decode_trans },               /* 66   42 */
        {"style", svgb_decode_fail },                    /* 67   43 */
        {"fill", svgb_decode_fail },                     /* 68   44 */
        {"stroke", svgb_decode_fail },                   /* 69   45 */

        {"color", svgb_decode_fail },                    /* 70   46 */
        {"from", svgb_decode_fail },                     /* 71   47 */
        {"to", svgb_decode_fail },                       /* 72   48 */
        {"by", svgb_decode_fail },                       /* 73   49 */
        {"attributeName", svgb_decode_un },              /* 74   4A */
        {"pathLength", svgb_decode_fail },               /* 75   4B */
        {"version", svgb_decode_float },		    /* 76   4C */
        {"strokeWidth", svgb_decode_fail },              /* 77   4D */
        {"points", svgb_decode_points },                 /* 78   4E */
        {"d", svgb_decode_path },                        /* 79   4F */
        {"type", svgb_decode_type },                     /* 80   50 */

        {"stop-color", svgb_decode_css_color },           /* 81 */
        {"fx", svgb_decode_float },                      /* 82 */
        {"fy", svgb_decode_float },                      /* 83 */
        {"offset", svgb_decode_float },                  /* 84 */
        {"spreadMethods", svgb_decode_spread_method },    /* 85 */
        {"gradientUnits", svgb_decode_grad_units },       /* 86 */
        {"stopOpacity", svgb_decode_float },             /* 87 */
        {"viewBox", svgb_decode_rect },                  /* 88 */
        {"baseProfile", svgb_decode_string },            /* 89 */

        {"zoomAndPan", svgb_decode_un },		    /* 90 */
        {"preserveAspectRatio", svgb_decode_string },    /* 91 */
        {"id", svgb_decode_string },                     /* 92 */
        {"xml:base", svgb_decode_string },               /* 93 */
        {"xml:lang", svgb_decode_string },               /* 94 */
        {"xml:space", svgb_decode_string },              /* 95 */
        {"requiredExtensions", svgb_decode_fail },       /* 96 */
        {"requiredFeatures", svgb_decode_fail },         /* 97 */
        {"systemLanguage", svgb_decode_fail },           /* 98 */
        {"dx", svgb_decode_fail },                       /* 99 */

        {"dy", svgb_decode_fail },                       /* 100 */
        {"media", svgb_decode_fail },                    /* 101 */
        {"title", svgb_decode_fail },                    /* 102 */
        {"xlink:actuate", svgb_decode_string },          /* 103 */
        {"xlink:arcrole", svgb_decode_fail },            /* 104 */
        {"xlink:role", svgb_decode_fail },               /* 105 */
        {"xlink:show", svgb_decode_string },             /* 106 */
        {"xlink:title", svgb_decode_fail },              /* 107 */
        {"xlink:type", svgb_decode_string },             /* 108 */
        {"xlink:href", svgb_decode_href },               /* 109 */

        {"begin", svgb_decode_fail },                    /* 110 */
        {"dur", svgb_decode_fail },                      /* 111 */
        {"repeatCount", svgb_decode_fail },              /* 112 */
        {"repeatDur", svgb_decode_fail },                /* 113 */
        {"end", svgb_decode_fail },                      /* 114 */
        {"restart", svgb_decode_fail },                  /* 115 */
        {"accumulate", svgb_decode_fail },               /* 116 */
        {"additive", svgb_decode_fail },                 /* 117 */
        {"keySplines", svgb_decode_fail },               /* 118 */
        {"keyTimes", svgb_decode_fail },                 /* 119 */
        {"calcMode", svgb_decode_fail },                 /* 120 */
        {"path", svgb_decode_fail },                     /* 121 */
        {"animateMotion", svgb_decode_fail },            /* 122 */
        {"gradientTransform", svgb_decode_grad_trans },   /* 123 */
        {"UNCNOUN", svgb_decode_fail },                  /* 124 */
        {"UNCNOUN", svgb_decode_fail },                  /* 125 */
        {"UNCNOUN", svgb_decode_fail },                  /* 126 */
        {"UNCNOUN", svgb_decode_fail },                  /* 127 */
        {"UNCNOUN", svgb_decode_fail },                  /* 128 */
        {"UNCNOUN", svgb_decode_fail },                  /* 129 */
        {"UNCNOUN", svgb_decode_fail },                  /* 130 */
        {"UNCNOUN", svgb_decode_fail },                  /* 131 */
        {"UNCNOUN", svgb_decode_fail },                  /* 132 */
        {"UNCNOUN", svgb_decode_fail },                  /* 133 */
        {"UNCNOUN", svgb_decode_fail },                  /* 134 */
        {"UNCNOUN", svgb_decode_fail },                  /* 135 */
        {"xlink:href", svgb_decode_lhref }               /* 136 */
    };

    #define SVG_ELEMENT_SVG 0

    static const svg_element_attr SVG_ATTR_SVG [] =
    {
        {"xmlns", "http://www.w3.org/2000/svg"},
        {"xmlns:xlink", "http://www.w3.org/1999/xlink"}
    };

    #define COUNT(array) ((int)(sizeof(array)/sizeof(array[0])))

    static const svg_element SVGB_ELEMS [] =
    {
        {"svg", SVG_ELEMENT_SVG, SVG_ATTR_SVG, COUNT(SVG_ATTR_SVG)},  /* 0 */
        {"altglyph", 1, nullptr, 0},                               /* 1 */
        {"altglyphdef", 2, nullptr, 0},                            /* 2 */
        {"defs", 3, nullptr, 0},                                   /* 3 */
        {"desc", 4, nullptr, 0},                                   /* 4 */

        {"foreignObject", 5, nullptr, 0},                          /* 5 */
        {"metadata", 6, nullptr, 0},                               /* 6 */
        {"title", 7, nullptr, 0},                                  /* 7 */
        {"fontfacename", 8, nullptr, 0},                           /* 8 */
        {"fontfacesrc", 9, nullptr, 0},                            /* 9 */

        {"fontfaceuri", 10, nullptr, 0},                           /* 10 */
        {"g", 11, nullptr, 0},                                     /* 11 */
        {"glyphref", 12, nullptr, 0},                              /* 12 */
        {"vkern", 13, nullptr, 0},                                 /* 13 */
        {"script", 14, nullptr, 0},                                /* 14 */

        {"switch", 15, nullptr, 0},                                /* 15 */
        {"view", 16, nullptr, 0},                                  /* 16 */
        {"hkern", 17, nullptr, 0},                                 /* 17 */
        {"a", 18, nullptr, 0},                                     /* 18 */
        {"font", 19, nullptr, 0},                                  /* 19 */

        {"fontface", 20, nullptr, 0},                              /* 20 */
        {"glyph", 21, nullptr, 0},                                 /* 21 */
        {"image", 22, nullptr, 0},                                 /* 22 */
        {"missingglyph", 23, nullptr, 0},                          /* 23 */
        {"style", 24, nullptr, 0},                                 /* 24 */

        {"text", 25, nullptr, 0},                                  /* 25 */
        {"use", 26, nullptr, 0},                                   /* 26 */
        {"circle", 27, nullptr, 0},                                /* 27 */
        {"ellipse", 28, nullptr, 0},                               /* 28 */
        {"line", 29, nullptr, 0},                                  /* 29 */

        {"path", 30, nullptr, 0},                                  /* 30 */
        {"polygon", 31, nullptr, 0},                               /* 31 */
        {"polyline", 32, nullptr, 0},                              /* 32 */
        {"rect", 33, nullptr, 0},                                  /* 33 */
        {"animate", 34, nullptr, 0},                               /* 34 */

        {"animateColor", 35, nullptr, 0},                          /* 35 */
        {"animateMotion", 36, nullptr, 0},                         /* 36 */
        {"animateTransform", 37, nullptr, 0},                      /* 37 */
        {"set", 38, nullptr, 0},                                   /* 38 */
        {"mpath", 39, nullptr, 0},                                 /* 39 */

        {"linearGradient", 40, nullptr, 0},                        /* 40 */
        {"radialGradient", 41, nullptr, 0},                        /* 41 */
        {"stop", 42, nullptr, 0}                                   /* 42 */
    };

    /**
     * Writes <tag to the output stream
     */
    bool xml_start_tag(svgb_decode_state &state, common::wo_stream &out, std::string tag) {
        std::string indent(state.wrap_level_, ' ');
        indent += "<" + tag;

        return out.write_text(indent);
    }

    bool xml_close_tag(svgb_decode_state &state, common::wo_stream &out, std::string tag, bool eol) {
        state.wrap_indent(-1);

        std::string indent(state.wrap_level_, ' ');
        indent += "</" + tag + (eol ? ">\xD\xA" : ">") + "\n";

        return out.write_text(indent);
    }

    /**
     * Writes an attribute to the output stream. Does not escape non-printable
     * characters.
     */
    bool xml_write_attr_no_esc(common::wo_stream &out, std::string attr, std::string value) {
        return out.write_text(fmt::format(" {}=\"{}\"", attr, value));
    }

    /**
     * Formats a single precision floating point number
     */
    static std::string svgb_fomat_float(std::string* buf, float val, bool append)
    {
        if (!append)
            buf->clear();

        std::string tmp = fmt::format("{:.4f}", static_cast<double>(val));

        if (tmp.substr(tmp.length() - 5) == ".0000")
            tmp = tmp.substr(0, tmp.length() - 5);
        else
            if (tmp.substr(tmp.length() - 4) == "000")
                tmp = tmp.substr(0, tmp.length() - 4);
        else
            if (tmp.substr(tmp.length() - 3) == "00")
                tmp = tmp.substr(0, tmp.length() - 3);
        else
            if (tmp.substr(tmp.length() - 2) == "00")
                tmp = tmp.substr(0, tmp.length() - 2);
        else
            if (tmp.substr(tmp.length() - 1) == "0")
                tmp = tmp.substr(0, tmp.length() - 1);

        buf->append(tmp);

        return *buf;
    }

    bool read_bytes_from_stream(common::ro_stream &in, char* ch, int count) {
        bool ok = true;

        for(int i = 0; i < count; i++) {
            if (in.read(&ch[i], 1) != 1) {
                ok = false;
            }
        }

        return ok;
    }

    bool read_f32l_from_stream(common::ro_stream &in, float* data) {
        return (in.read(data, 4) == 4);
    }

    bool read_i16l_from_stream(common::ro_stream &in, std::int16_t* data) {
        return (in.read(data, 2) == 2);
    }

    bool read_i32l_from_stream(common::ro_stream &in, std::int32_t* data) {
        return (in.read(data, 4) == 4);
    }

    bool read_f32_from_stream(svgb_decode_state &state, common::ro_stream &in, float* data) {
        if ((state.is_new_ == 1) || (state.is_new_ == 2)) {
            // Two parts separated, integer and real part
            std::uint16_t real;
            std::int16_t integ;

            if (in.read(&real, 2) != 2) {
                return false;
            }

            if (in.read(&integ, 2) != 2) {
                return false;
            }

            *data = integ + (static_cast<float>(real) /0xFFFF);
            return true;
        }

        return read_f32l_from_stream(in, data);
    }

    std::optional<std::string> read_svgb_string_from_stream(common::ro_stream &in) {
        char len;
        if (in.read(&len, 1) != 1) {
            return std::nullopt;
        }

        std::string final(len, '\0');
        if (in.read(final.data(), len) != len) {
            return std::nullopt;
        }

        return final;
    }

    std::optional<std::string> read_svgb_lstring_from_stream(common::ro_stream &in) {
        int len;
        if (in.read(&len, 4) != 4) {
            return std::nullopt;
        }

        std::string final(len, '\0');
        if (in.read(final.data(), len) != len) {
            return std::nullopt;
        }

        return final;
    }


    /**
     * Writes a single precision floating point attribute to the output stream.
     * Almost like XML_WriteFloatAttr, but with less precision.
     */
    static bool svgb_write_float_attr(common::wo_stream &out, std::string attr, float val, bool percent)
    {
        std::string text;
        text = svgb_fomat_float(&text, val, false);
        text = fmt::format(" {}=\"{}{}\"", attr, text, percent ? "%" : "");

        return out.write_text(text);
    }

    static bool svgb_read_write_float_attr(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        float value;
        if (read_f32_from_stream(state, in, &value))
        {
            svgb_write_float_attr(out, attr->name_, value, false);
            return true;
        }

        state.add_error(in, svgb_convert_error_eof);
        return false;
    }

    /**
     * Placeholder for attributes that are not implemented
     */
    static bool svgb_decode_dash_array(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        char len;
        std::string buf;

        if (in.read(&len, 1) != 1) {
            state.add_error(in, svgb_convert_error_eof);
            return false;
        }

        for (char i = 0; i < len; i++) {
            float value;
            if (!read_f32_from_stream(state, in, &value)) {
                state.add_error(in, svgb_convert_error_eof);
                return false;
            }

            if (i > 0)
                buf += ',';

            svgb_fomat_float(&buf, value, true);
        }

        if (!xml_write_attr_no_esc(out, attr->name_, buf)) {
            state.add_error(in, svgb_convert_error_write_fail);
            return false;
        }

        return true;
    }

    /**
     * Placeholder for attributes that are not implemented
     */
    static bool svgb_decode_fail(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        state.add_error(in, svgb_convert_error_element_unimplemented, attr->name_);
        return false;
    }


    static bool svgb_decode_un(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        char i;
        if (in.read(&i, 1) != 1) {
            state.add_error(in, svgb_convert_error_eof);
            return false;
        }

        if (i == 1 && (attr->name_ == "stroke-dasharray"))
            in.seek(4, common::seek_where::cur);

        return true;
    }

    static bool svgb_decode_href(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        std::optional<std::string> str = read_svgb_string_from_stream(in);
        if (str.has_value()) {
            xml_write_attr_no_esc(out, attr->name_, str.value());

            // Next comes another string, usually the same as the first one
            char len;
            if (in.read(&len, 1) != 1) {
                state.add_error(in, svgb_convert_error_eof);
                return false;
            }

            if (len > 0)
                in.seek(len, common::seek_where::cur);

            return true;
        }

        state.add_error(in, svgb_convert_error_eof);
        return false;
    }

    static bool svgb_decode_lhref(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        std::optional<std::string> str = read_svgb_lstring_from_stream(in);
        if (str.has_value()) {
            xml_write_attr_no_esc(out, attr->name_, str.value());
            return true;
        }

        state.add_error(in, svgb_convert_error_eof);
        return false;
    }

    /**
     * XX           - flag (0 == color, 1 == URL)
     * followed by either 32-bit color or URL string
     */
    static bool svgb_decode_fill(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        std::int8_t flag = 0;
        if (!in.read(&flag, 1)) {
            return false;
        }

        std::uint8_t color[4];

        switch (flag) {
        case 0:
            if (read_bytes_from_stream(in, reinterpret_cast<char*>(color), 4))
            {
                int i;
                const svg_fill * fill = nullptr;
                for (i=0; i< COUNT(SVGB_FILL); i++) {
                    if (!memcmp(SVGB_FILL[i].value_, color, sizeof(color))) {
                        fill = SVGB_FILL + i;
                        break;
                    }
                }
                if (fill) {
                    xml_write_attr_no_esc(out, attr->name_, fill->name_);
                } else {
                    std::string buf;

                    std::string color0 = fmt::format("{:02X}", color[0]);
                    std::string color1 = fmt::format("{:02X}", color[1]);
                    std::string color2 = fmt::format("{:02X}", color[2]);

                    if ((state.is_new_ == 0) || (state.is_new_ == 1))
                        buf = fmt::format("#{}{}{}", color0, color1, color2);
                    else
                        buf = fmt::format("#{}{}{}", color2, color1, color0);

                    xml_write_attr_no_esc(out, attr->name_, buf);
                }
                return true;
            }


            break;

        case 1:
            {
                std::optional<std::string> url_content = read_svgb_string_from_stream(in);
                std::u16string data(reinterpret_cast<char16_t*>(url_content->data()), url_content->size() / 2);

                xml_write_attr_no_esc(out, attr->name_, "url(#" + common::ucs2_to_utf8(data) + ")");

                return true;
            }
        default:
            break;
        }

        state.add_error(in, svgb_convert_error_unsupport_fill_flag, fmt::format("{}", flag));
        return false;
    }

    /**
     * XX XX XX XX  - Color
     */
    static bool svgb_decode_css_color(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out)
    {
        std::uint8_t color[4];
        if (read_bytes_from_stream(in, reinterpret_cast<char*>(color),4))
        {
            std::string buf;

            std::string color0 = fmt::format("{:02X}", color[0]);
            std::string color1 = fmt::format("{:02X}", color[1]);
            std::string color2 = fmt::format("{:02X}", color[2]);

            if ((state.is_new_ == 0) || (state.is_new_ == 1))
                buf = fmt::format("#{}{}{}", color0, color1, color2);
            else
                buf = fmt::format("#{}{}{}", color2, color1, color0);


            xml_write_attr_no_esc(out, attr->name_, buf);
            return true;
        }

        state.add_error(in, svgb_convert_error_eof);
        return false;
    }

    /**
     * XX  - enum value
     */
    static bool svgb_decode_enum(svgb_decode_state &state, const svg_attr * attr, common::ro_stream &in, common::wo_stream &out, const std::string values[], int n) {
        std::int8_t flag;
        if (in.read(&flag, 1) != 1) {
            state.add_error(in, svgb_convert_error_eof);
            return false;
        }

        if (flag >= 0) {
            if (flag < n) {
                xml_write_attr_no_esc(out, attr->name_, values[flag]);
            } else {
                state.add_error(in, svgb_convert_error_unexpected_value, fmt::format("{}", flag), attr->name_);
            }

            return true;
        }

        state.add_error(in, svgb_convert_error_eof);
        return false;
    }

    static bool svgb_decode_spread_method(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        static const std::string values[] = {"pad", "reflect","repeat"};
        return svgb_decode_enum(state, attr, in, out, values, COUNT(values));
    }

    static bool svgb_decode_grad_units(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        static const std::string values[] = {"userSpaceOnUse", "objectBoundingBox"};
        return svgb_decode_enum(state, attr, in, out, values, COUNT(values));
    }

    /**
     * XX XX XX XX - 32-bit enum value
     */
    static bool svgb_decode_css_enum(svgb_decode_state &state, const svg_attr * attr, common::ro_stream &in, common::wo_stream &out, const std::string values[], std::int32_t n, std::string defval) {
        std::int32_t i;
        if (read_i32l_from_stream(in, &i)) {
            if (i < n) {
                xml_write_attr_no_esc(out, attr->name_, values[i]);
            } else if (!defval.empty()) {
                xml_write_attr_no_esc(out, attr->name_, defval);
            } else {
                state.add_error(in, svgb_convert_error_unexpected_value, fmt::format("{}", i), attr->name_);
            }

            return true;
        }

        state.add_error(in, svgb_convert_error_eof);
        return false;
    }

    static bool svgb_decode_display(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        static const std::string values[] = {"inline", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "none"};
        return svgb_decode_css_enum(state, attr, in, out, values, COUNT(values), "inherit");
    }

    static bool svgb_decode_visibility(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        static const std::string values[] = {"visible", "hidden"};
        return svgb_decode_css_enum(state, attr, in, out, values, COUNT(values), nullptr);
    }

    static bool svgb_decode_font_style(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        static const std::string values[] = {"normal", "italic", "oblique", "inherit"};
        return svgb_decode_css_enum(state, attr, in, out, values, COUNT(values), nullptr);
    }

    static bool svgb_decode_font_weight(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        static const std::string values[] = {"normal", "bold", "bolder", "lighter",
                                         "100", "200", "300", "400", "500", "600", "700", "800", "900",
                                         "inherit"};
        return svgb_decode_css_enum(state, attr, in, out, values, COUNT(values), nullptr);
    }

    static bool svgb_decode_text_anchor(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        static const std::string values[] = {"start", "middle", "end"};
        return svgb_decode_css_enum(state, attr, in, out, values, COUNT(values), nullptr);
    }

    static bool svgb_decode_text_decor(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        static const std::string values[]={"none","underline","overline","line-through"};
        return svgb_decode_css_enum(state, attr, in, out, values, COUNT(values), nullptr);
    }

    /**
     * XX  - size in bytes
     * XXXX XXXX - UCS2 string
     */
    static bool svgb_decode_string(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        std::optional<std::string> value = read_svgb_string_from_stream(in);
        if (!value.has_value()) {
            state.add_error(in, svgb_convert_error_eof);
            return false;
        }

        std::u16string data(reinterpret_cast<char16_t*>(value->data()), value->size() / 2);

        if (!xml_write_attr_no_esc(out, attr->name_, common::ucs2_to_utf8(data))) {
            state.add_error(in, svgb_convert_error_write_fail);
            return false;
        }

        return true;
    }

    /**
     * XX XX            - number of segments
     * XX * n           - segment types
     * XX XX            - number of 32 bit floating point values following
     * XX XX XX XX * n  - 32 bit floating point values
     */
    static bool svgb_decode_path(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        std::int16_t nseg;
        if (read_i16l_from_stream(in, &nseg)) {
            std::vector<char> seg(nseg);
            if (read_bytes_from_stream(in, seg.data(), nseg)) {
                std::int16_t nval;
                if (read_i16l_from_stream(in, &nval)) {
                    std::uint16_t iseg, ival = 0;
                    std::string buf;

                    for (iseg=0; iseg<nseg; iseg++) {
                        if (seg[iseg] < COUNT(SVG_PATH_SEG)) {
                            int i;
                            const svg_path_seg_type * s = SVG_PATH_SEG+seg[iseg];
                            buf += s->command_;
                            for(i=0; i<s->params_; i++, ival++) {
                                float val;
                                if (ival >= nval) {
                                    return false;
                                }
                                if (!read_f32_from_stream(state, in, &val)) {
                                    state.add_error(in, svgb_convert_error_eof);
                                    return false;
                                }
                                if (i > 0 && val >= 0) {
                                    buf += ',';
                                }
                                svgb_fomat_float(&buf, val, true);
                            }
                        } else {
                            return false;
                        }
                    }
                    if (!xml_write_attr_no_esc(out, attr->name_,buf)) {
                        state.add_error(in, svgb_convert_error_write_fail);
                        return false;
                    }
                    return true;
                }
            }
        }

        state.add_error(in, svgb_convert_error_eof);
        return false;
    }

    /**
     * XX XX            - number of bytes following (n)
     * XX * n           - bytes (segment types?)
     * XX XX            - number of 32 bit floating point values following
     * XX XX XX XX * n  - 32 bit floating point values, 2 for each point
     */
    static bool svgb_decode_points(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        std::int16_t nflags;
        if (read_i16l_from_stream(in, &nflags)) {
            std::vector<char> flags(nflags);
            if (read_bytes_from_stream(in, flags.data(), nflags)) {
                std::int16_t nval;
                if (read_i16l_from_stream(in, &nval)) {
                    std::int16_t i;
                    std::string buf;
                    for (i=0; i<nval/2; i++) {
                        float x, y;
                        if (!read_f32_from_stream(state, in, &x) || !read_f32_from_stream(state, in, &y)) {
                            state.add_error(in, svgb_convert_error_eof);
                            return false;
                        }
                        if (i > 0) buf += ' ';
                        svgb_fomat_float(&buf, x, true);
                        buf += ' ';
                        svgb_fomat_float(&buf, y, true);
                    }

                    if (!xml_write_attr_no_esc(out, attr->name_, buf)) {
                        state.add_error(in, svgb_convert_error_write_fail);
                        return false;
                    }

                    return true;
                }
            }
        }

        state.add_error(in, svgb_convert_error_eof);
        return false;
    }

    static bool svgb_decode_type(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        // TODO: This has so much more to do with animate transform element
        return svgb_decode_string(state, attr, elem, in, out);
    }

    /**
     * XX XX XX XX  - 32 bit floating point value (matrix[0][0])
     * XX XX XX XX  - 32 bit floating point value (matrix[0][1])
     * XX XX XX XX  - 32 bit floating point value (matrix[0][2])
     * XX XX XX XX  - 32 bit floating point value (matrix[1][0])
     * XX XX XX XX  - 32 bit floating point value (matrix[1][1])
     * XX XX XX XX  - 32 bit floating point value (matrix[1][2])
     */
    static bool svgb_decode_matrix(svgb_decode_state &state, const svg_attr * attr, common::ro_stream &in, common::wo_stream &out, float divisor) {
        int i;
        std::string buf;

        float matrix[6];
        static const int trans[6] = {0, 3, 1, 4, 2, 5};

        for (i=0; i<COUNT(matrix); i++) {
            if (read_f32_from_stream(state, in, matrix+i)) {
                if(!state.is_new_)
                    matrix[i] /= divisor;
            } else {
                state.add_error(in, svgb_convert_error_eof);
                return false;
            }
        }

        buf.append("matrix(");
        for (i=0; i<COUNT(matrix); i++) {
            if (i > 0) buf += ' ';
            svgb_fomat_float(&buf, matrix[trans[i]], true);
        }
        buf.append(")");

        if (!xml_write_attr_no_esc(out, attr->name_, buf)) {
            state.add_error(in, svgb_convert_error_write_fail);
            return false;
        }

        return true;
    }

    /**
     * XX XX XX XX  - 32 bit floating point value (matrix[0][0])
     * XX XX XX XX  - 32 bit floating point value (matrix[0][1])
     * XX XX XX XX  - 32 bit floating point value (matrix[0][2])
     * XX XX XX XX  - 32 bit floating point value (matrix[1][0])
     * XX XX XX XX  - 32 bit floating point value (matrix[1][1])
     * XX XX XX XX  - 32 bit floating point value (matrix[1][2])
     */
    static bool svgb_decode_grad_trans(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        return svgb_decode_matrix(state, attr, in, out, 65535.0f);
    }

    /**
     * XX XX XX XX  - 32 bit floating point value (matrix[0][0])
     * XX XX XX XX  - 32 bit floating point value (matrix[0][1])
     * XX XX XX XX  - 32 bit floating point value (matrix[0][2])
     * XX XX XX XX  - 32 bit floating point value (matrix[1][0])
     * XX XX XX XX  - 32 bit floating point value (matrix[1][1])
     * XX XX XX XX  - 32 bit floating point value (matrix[1][2])
     * XX XX XX XX  - transform type?
     */
    static bool svgb_decode_trans(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        if (svgb_decode_matrix(state, attr, in, out, 1.0f)) {
            std::int32_t type;
            read_i32l_from_stream(in,&type);
            return true;
        }

        return false;
    }

    /**
     * XX XX XX XX  - 32 bit floating point value (X)
     * XX XX XX XX  - 32 bit floating point value (Y)
     * XX XX XX XX  - 32 bit floating point value (Width)
     * XX XX XX XX  - 32 bit floating point value (Height)
     */
    static bool svgb_decode_rect(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        int i;
        std::string buf;

        for (i=0; i<4; i++)
        {
            float x;
            if (!read_f32_from_stream(state, in, &x)) {
                state.add_error(in, svgb_convert_error_eof);
                return false;
            }

            if (i > 0) buf += ' ';
            svgb_fomat_float(&buf, x, true);
        }

        if (!xml_write_attr_no_esc(out, attr->name_, buf)) {
            state.add_error(in, svgb_convert_error_write_fail);
        }

        return true;
    }

    /**
     * XX XX XX XX  - 32 bit floating point value
     */
    static bool svgb_decode_float(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        return svgb_read_write_float_attr(state,attr,elem,in,out);
    }

    /**
     * Need special case for opacity attribute?
     */
    static bool svgb_decode_opacity(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out) {
        return svgb_decode_float(state, attr, elem, in, out);
    }


    /**
     * XX           - 1 for percent, 0 otherwise (root element only)
     * XX XX XX XX  - 32 bit floating point value
     */
    static bool svgb_decode_size(svgb_decode_state &state, const svg_attr * attr, const svg_element * elem, common::ro_stream &in, common::wo_stream &out)
    {
        float value;
        char flag = 0;

        if (elem->code_ == SVG_ELEMENT_SVG) {
            if (in.read(&flag, 1) != 1) {
                state.add_error(in, svgb_convert_error_eof);
                return false;
            }
        }

        if (read_f32_from_stream(state, in, &value)) {
            svgb_write_float_attr(out, attr->name_, value, (flag != 0));
            return true;
        }

        return svgb_read_write_float_attr(state, attr, elem, in, out);
    }

    /**
     * Decodes a single element.
     */
    static svg_decode_elem_status svgb_decode_element(svgb_decode_state &state, const svg_element * elem, common::ro_stream &in, common::wo_stream &out)
    {
        int i;
        char endmark[2];

        /* Add implied attributes */
        if (!xml_start_tag(state, out, elem->name_)) {
            state.add_error(in, svgb_convert_error_write_fail);
            return svg_decode_error;
        }

        for (i=0; i < elem->nattr_; i++)
        {
            if (!xml_write_attr_no_esc(out, elem->attr_[i].name_, elem->attr_[i].value_)) {
                state.add_error(in, svgb_convert_error_write_fail);
                return svg_decode_error;
            }
        }

        /* Loop until we hit SVG_ATR_END mark */
        while (read_bytes_from_stream(in,endmark,2)) {
            std::int16_t id = 0;
            if (!memcmp(endmark, SVG_ATR_END, sizeof(SVG_ATR_END))) {
                std::uint8_t next;
                if (in.read(&next, 1) == 1) {
                    if (next == SVG_ELEMENT_END) {
                        out.write_text("/>\xD\xA");
                        return svg_decode_elem_finished;
                    } else {
                        in.seek(-1, common::seek_where::cur);
                        out.write_text((next == SVG_CDATA) ? ">" : ">\xD\xA");
                        state.wrap_indent(+1);
                        return svg_decode_elem_started;
                    }
                } else {
                    state.add_error(in, svgb_convert_error_eof);
                    return svg_decode_error;
                }
            }

            /* Handle next attribute */
            in.seek(-static_cast<std::int64_t>(sizeof(endmark)), common::seek_where::cur);
            if (!read_i16l_from_stream(in, &id)) {
                state.add_error(in, svgb_convert_error_eof);
                return svg_decode_error;
            }

            if (id < COUNT(SVGB_ATTRS)) {
                const svg_attr * a = SVGB_ATTRS + id;
                if (!a->decode_(state, a, elem, in, out)) {
                    return svg_decode_error;
                }

            } else {
                state.add_error(in, svgb_convert_error_unexpected_attribute, fmt::format("{}", id));
                break;
            }

        }

        return svg_decode_error;
    }

    /**
     * Decodes SVGB stream, writes XML to the output stream.
     */
    bool convert_svgb_to_svg(common::ro_stream &in, common::wo_stream &out, std::vector<svgb_convert_error_description> &errors) {
        svgb_decode_state decode_state(errors);

        std::uint32_t start = 0;
        if (in.read(&start, 4) != 4) {
            decode_state.add_error(in, svgb_convert_error_eof);
            return false;
        }

        if ((start == SVG_FILE_START) || (start == SVG_FILE_START2) || (start == SVG_FILE_START3) || (start == SVG_FILE_START4)) {
            if (start == SVG_FILE_START2)
                decode_state.is_new_ = 1;

            if (start == SVG_FILE_START3)
                decode_state.is_new_ = 2;

            if (start == SVG_FILE_START4)
                decode_state.is_new_ = 3;

            if (!out.write_text(SVG_START)) {
                decode_state.add_error(in, svgb_convert_error_write_fail);
                return false;
            }

            std::uint8_t c;
            std::stack<std::string> stack;

            while (in.read(&c, 1) == 1) {
                if (c == SVG_FILE_END) {
                    return true;
                } else if (c == SVG_ELEMENT_END) {
                    decode_state.wrap_indent(-1);

                    const std::string elem_name = stack.top();
                    stack.pop();

                    if (!xml_close_tag(decode_state, out, elem_name, true)) {
                        decode_state.add_error(in, svgb_convert_error_write_fail);
                        return false;
                    }
                } else if (c == SVG_CDATA) {/*
                    char * str = read_svgb_string_from_stream(in);
                    if (str) {
                        Bool wrap = WRAP_IsEnabled(out);
                        if (wrap) WRAP_Enable(out, False);
                        FILE_Puts(out, str);
                        if (wrap) WRAP_Enable(out, True);
                        MEM_Free(str);
                    } else {
                        return False;
                        */
                    decode_state.add_error(in, svgb_convert_error_cdata_ignored);
                } else if (c < COUNT(SVGB_ELEMS)) {
                    const svg_element * elem = SVGB_ELEMS + c;
                    const svg_decode_elem_status s = svgb_decode_element(decode_state, elem, in, out);

                    switch (s)
                    {
                    case svg_decode_elem_started: {
                        decode_state.wrap_indent(1);
                        stack.push(elem->name_);
                    }
                    case svg_decode_elem_finished:
                        break;

                    case svg_decode_error:
                    default:
                        return false;
                    }
                } else {
                    decode_state.add_error(in, svgb_convert_error_unexpected_element, fmt::format("{}", c));
                    return false;
                }
            }
        } else {
            decode_state.add_error(in, svgb_convert_error_invalid_file);
            return false;
        }

        return false;
    }
}
