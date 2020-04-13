/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <epoc/services/fbs/font_store.h>

namespace eka2l1::epoc {
    void font_store::add_fonts(std::vector<std::uint8_t> &buf) {
        auto adapter = epoc::adapter::make_font_file_adapter(epoc::adapter::font_file_adapter_kind::stb, buf);

        for (std::size_t i = 0; i < adapter->count(); i++) {
            epoc::open_font_face_attrib attrib;

            if (!adapter->get_face_attrib(i, attrib)) {
                continue;
            }

            const std::u16string fam_name = attrib.fam_name.to_std_string(nullptr);
            const std::u16string name = attrib.name.to_std_string(nullptr);
            auto &font_fam_collection = open_font_store[fam_name];

            bool found = false;

            // Are we facing duplicate fonts ?
            for (std::size_t i = 0; i < font_fam_collection.size(); i++) {
                if (font_fam_collection[i].face_attrib.name.to_std_string(nullptr) == name) {
                    found = true;
                    break;
                }
            }

            // No duplicate font
            if (!found) {
                // Get the metrics and make new open font
                open_font_info info;
                adapter->get_metrics(i, info.metrics);

                info.idx = static_cast<std::int32_t>(i);
                info.face_attrib = attrib;
                info.adapter = adapter.get();

                font_fam_collection.push_back(std::move(info));
            }
        }

        font_adapters.push_back(std::move(adapter));
    }

    open_font_info *font_store::seek_the_open_font(epoc::font_spec &spec) {
        open_font_info *best = nullptr;
        int best_score = -99999999;

        const std::u16string my_name = spec.tf.name.to_std_string(nullptr);

        for (auto &[fam_name, collection] : open_font_store) {
            bool maybe_same_family = (my_name.find(fam_name) != std::string::npos);

            // Seek the way out! Seek the one that contains my name first!
            for (auto &info : collection) {
                int score = 0;

                // Better returns me!
                if (info.face_attrib.name.to_std_string(nullptr) == my_name) {
                    return &info;
                }

                if (maybe_same_family) {
                    score += 100;
                }

                if ((spec.style.flags & epoc::font_style::italic) == (info.face_attrib.style & epoc::open_font_face_attrib::italic)) {
                    score += 50;
                }

                if ((spec.style.flags & epoc::font_style::bold) == (info.face_attrib.style & epoc::open_font_face_attrib::bold)) {
                    score += 50;
                }

                if (score > best_score) {
                    best = &info;
                    best_score = score;
                }
            }
        }

        return best;
    }
}