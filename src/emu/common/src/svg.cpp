/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <common/algorithm.h>
#include <common/color.h>
#include <common/paint.h>
#include <common/pystr.h>
#include <common/svg.h>

// Dependency
#include <pugixml.hpp>

#include <unordered_map>

namespace eka2l1::common {
    /**
     * \briefe Set the SVG diag string. If a null pointer is passed, nothing is set.
     * 
     * \param diag Pointer to diag string.
     * \param err  Target error string to set.
     */
    static void svg_set_diag(std::string *diag, const char *err) {
        (diag) ? *diag = err : 0;
    }

    /**
     * \brief Get SVG viewpoint/canavas size.
     * 
     * \param doc      Reference to PugiXML DOM tree.
     * \param svg_tag  Reference to SVG node. Will be a node with <svg> tag on success.
     * \param width    Reference to the width variable.
     * \param height   Reference to the height variable.
     * \param diag     Pointer to diag string.
     */
    static int svg_get_canavas_size(pugi::xml_document &doc, pugi::xml_node &svg_tag, int &width, int &height, 
        std::string *diag) {
        // Looking for a <svg> tag first
        svg_tag = doc.child("svg");

        // We can't find the tag
        if (!svg_tag) {
            svg_set_diag(diag, "Can't found the <svg> tag! Viewpoint not determined!");
            return svg_err_not_found;
        }

        pugi::xml_attribute width_attr = svg_tag.attribute("width");
        pugi::xml_attribute height_attr = svg_tag.attribute("height");

        if (width_attr && height_attr) {
            width = width_attr.as_int();
            height = width_attr.as_int();
        } else {
            // Get the viewBox attribute
            pugi::xml_attribute viewbox_attr = svg_tag.attribute("viewBox");

            if (!viewbox_attr) {
                svg_set_diag(diag, "Can't find either viewBox attribute or width/height attribute!");
                return svg_err_not_found;
            }

            // TODO: Process top left. Now we are just gonna get the width and height
            const common::pystr viewbox_str = viewbox_attr.as_string();
            const auto comps = viewbox_str.split(' ');

            width = comps[2].as_int<int>();
            height = comps[3].as_int<int>();
        }

        if (!width || !height) {
            svg_set_diag(diag, "Width and height attribute is invalid (size of 0 or not integer).");
            return svg_err_invalid;
        }
        
        return svg_err_ok;
    }

    static vecx<int, 4> make_rgba(const color::vec_rgb &col, const float alpha) {
        return { col[0], col[1], col[2], static_cast<int>(alpha * 255) };
    }

    /**
     * \brief Parse a string telling information about a color.
     * 
     * Supported kind:
     * - rgb(r, g, b)
     * - rgba(r, g, b, a)
     * - Color names.
     * 
     * \returns RGBA color.
     */
    static vecx<int, 4> svg_get_color(const common::pystr &color_str) {
        if (color_str.empty()) {
            return make_rgba(common::color::black, 1.0f);
        }

        const common::pystr func = color_str.substr(0, 4);
        int start_div_pos = -1;

        if (func == "rgba") {
            start_div_pos = 4;
        } else if (strncmp(func.data(), "rgb", 3) == 0) {
            start_div_pos = 3;
        } 

        if (start_div_pos != -1) {
            if (color_str[start_div_pos] != '(' || color_str.back() != ')') {
                // Invalid
                return make_rgba(common::color::black, 1);
            }

            auto tokens = color_str.substr(start_div_pos + 1, static_cast<int>(color_str.length()) - start_div_pos - 2).split(',');

            for (auto &token: tokens) {
                token = token.strip();
            }

            color::vec_rgb col_3 = { tokens[0].as_int<int>(), tokens[1].as_int<int>(), tokens[2].as_int<int>() };
            if (tokens.size() == 4) {
                return make_rgba(col_3, tokens[3].as_fp<float>());
            }

            return make_rgba(col_3, 1.0f);
        }

        common::color::vec_rgb col = common::color::get_color(color_str.data());
        return make_rgba(col, 1.0f);
    }

    /**
     * \brief Draw a circle.
     * 
     * \param command A XML node, contains circle drawing command.
     * \param painter The painter, which is used to draw SVG data.
     * \param diag    Pointer to diag string.
     * 
     * \returns svg_err_ok on success.
     */
    static int svg_cmd_circle(pugi::xml_node &command, common::painter *painter, std::string *diag) {
        // Get the origin
        eka2l1::vec2 origin { 0, 0 };
        origin.x = command.attribute("cx").as_int();
        origin.y = command.attribute("cy").as_int();

        const int radius = command.attribute("r").as_int();
        const int stroke = command.attribute("stroke-width").as_int(1);

        const auto stroke_color = svg_get_color(command.attribute("stroke").as_string());

        painter->set_fill_when_draw(true);
        painter->set_fill_color(svg_get_color(command.attribute("fill").as_string()));
        painter->set_brush_color(stroke_color);
        painter->set_brush_thickness(stroke);
        painter->circle(origin, radius);

        return svg_err_ok;
    }

    typedef int (*svg_cmd_func)(pugi::xml_node&,common::painter*,std::string*);
    static const std::unordered_map<std::string, svg_cmd_func> svg_cmd_funcs = {
        { "circle", svg_cmd_circle }
    };

    /**
     * \brief Process drawing command.
     * 
     * \param command A XML node, contains drawing command.
     * \param painter The painter, which is used to draw SVG data.
     * \param diag    Pointer to diag string.
     * 
     * \returns svg_err_ok on success.
     */
    static int svg_process_command(pugi::xml_node &command, common::painter *painter, std::string *diag) {
        const std::string cmd_name = common::lowercase_string(command.name());
        auto svg_cmd_func_pair = svg_cmd_funcs.find(cmd_name);

        if (svg_cmd_func_pair == svg_cmd_funcs.end()) {
            // Unimplemented or not handled
            const std::string err = "Unknown or unimplemented command: " + cmd_name;
            svg_set_diag(diag, err.data());

            return svg_err_invalid;
        }

        return svg_cmd_func_pair->second(command, painter, diag);
    }

    /**
     * \brief Iterates through all children of <svg> tag, and process drawing commands.
     * 
     * \param svg_tag A PugiXML node of <svg> tag.
     * \param painter The painter, which is used to draw SVG data.
     * \param diag    Pointer to diag string.
     * 
     * \returns svg_err_ok on success.
     */
    static int svg_process_commands(pugi::xml_node &svg_tag, common::painter *painter, std::string *diag) {
        for (pugi::xml_node command: svg_tag) {
            const int err = svg_process_command(command, painter, diag);

            if (err != svg_err_ok) {
                return err;
            }
        }

        return svg_err_ok;
    }

    int svg_render(pixel_plotter *plotter, const char *xml_data, std::string *diag) {
        common::painter painter_(plotter);

        pugi::xml_document dom_;
        const pugi::xml_parse_result result = dom_.load_string(xml_data);

        if (!result) {
            svg_set_diag(diag, result.description());
            return svg_err_invalid;
        }

        int width, height = 0;
        pugi::xml_node svg_tag;

        int err = svg_get_canavas_size(dom_, svg_tag, width, height, diag);

        if (err != svg_err_ok) {
            return err;
        }

        painter_.new_art({ width, height });

        return svg_process_commands(svg_tag, &painter_, diag);
    }
}
