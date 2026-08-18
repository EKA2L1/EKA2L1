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

#include <services/fbs/linked_font_config.h>

#include <common/cvt.h>

#include <map>

namespace eka2l1::epoc {
    struct linked_font_element {
        std::u16string linked_name;
        std::u16string component_name;
        bool canonical = false;
    };

    static std::u16string trim(const std::u16string &str) {
        std::size_t start = 0;
        std::size_t end = str.length();

        while ((start < end) && ((str[start] == u' ') || (str[start] == u'\t'))) {
            start++;
        }

        while ((end > start) && ((str[end - 1] == u' ') || (str[end - 1] == u'\t'))) {
            end--;
        }

        return str.substr(start, end - start);
    }

    // NOTE: S60's own parser (AknFontProvider's GetGroupCanonicalDetails) takes
    // every unrecognised token as the component name, so the "SNSemiBold" field
    // overwrites the typeface name it read moments earlier. Keep the first
    // unrecognised token instead: that is the field naming a typeface the
    // device actually ships.
    static bool parse_line(const std::u16string &line, linked_font_element &element) {
        std::size_t start = 0;
        bool has_name = false;

        while (start < line.length()) {
            std::size_t end = line.find(u':', start);

            if (end == std::u16string::npos) {
                end = line.length();
            }

            const std::u16string token = trim(line.substr(start, end - start));
            start = end + 1;

            if (token.empty()) {
                continue;
            }

            if (token.rfind(u"GROUP", 0) == 0) {
                continue;
            } else if (token.rfind(u"CANONICAL", 0) == 0) {
                element.canonical = (token.find(u'1') != std::u16string::npos);
            } else if (token.rfind(u"FN", 0) == 0) {
                element.linked_name = token.substr(2);
            } else if ((token == u"REGULAR") || (token == u"BOLD") || (token.rfind(u"SN", 0) == 0)) {
                continue;
            } else if (!has_name) {
                element.component_name = token;
                has_name = true;
            }
        }

        return has_name && !element.linked_name.empty();
    }

    std::u16string decode_linked_font_config(const std::uint8_t *data, const std::size_t size) {
        if ((size >= 2) && (data[0] == 0xFF) && (data[1] == 0xFE)) {
            return std::u16string(reinterpret_cast<const char16_t *>(data + 2), (size - 2) / sizeof(char16_t));
        }

        return common::utf8_to_ucs2(std::string(reinterpret_cast<const char *>(data), size));
    }

    std::vector<linked_font_spec> parse_linked_font_config(const std::u16string &content) {
        // The map only gathers the lines that belong together -- a section
        // interleaves the elements of several typefaces -- while `order` keeps
        // the typefaces in the order the file introduces them.
        std::vector<std::u16string> order;
        std::map<std::u16string, std::vector<linked_font_element>> elements;

        std::size_t line_start = 0;

        while (line_start <= content.length()) {
            std::size_t line_end = content.find_first_of(u"\r\n", line_start);

            if (line_end == std::u16string::npos) {
                line_end = content.length();
            }

            const std::u16string line = content.substr(line_start, line_end - line_start);
            line_start = line_end + 1;

            linked_font_element element;

            if ((line.find(u'[') == std::u16string::npos) && parse_line(line, element)) {
                if (elements.find(element.linked_name) == elements.end()) {
                    order.push_back(element.linked_name);
                }

                elements[element.linked_name].push_back(element);
            }
        }

        std::vector<linked_font_spec> specs;

        for (const auto &linked_name : order) {
            linked_font_spec spec;
            spec.name = linked_name;

            for (const auto &element : elements[linked_name]) {
                if (element.canonical) {
                    spec.canonical = spec.component_names.size();
                }

                spec.component_names.push_back(element.component_name);
            }

            specs.push_back(std::move(spec));
        }

        return specs;
    }
}
