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

#include <services/fbs/font_store.h>

namespace eka2l1::epoc {
    void font_store::add_fonts(std::vector<std::uint8_t> &buf, const epoc::adapter::font_file_adapter_kind adapter_kind) {
        auto adapter = epoc::adapter::make_font_file_adapter(adapter_kind, buf);

        if (!adapter->is_valid()) {
            return;
        }

        for (std::size_t i = 0; i < adapter->count(); i++) {
            epoc::open_font_face_attrib attrib;

            if (!adapter->get_face_attrib(i, attrib)) {
                continue;
            }

            const std::u16string fam_name = attrib.fam_name.to_std_string(nullptr);
            const std::u16string name = attrib.name.to_std_string(nullptr);

            bool found = false;

            // Are we facing duplicate fonts ?
            for (std::size_t i = 0; i < open_font_store.size(); i++) {
                if (open_font_store[i].face_attrib.name.to_std_string(nullptr) == name) {
                    found = true;
                    break;
                }
            }

            // No duplicate font
            if (!found) {
                // Get the metrics and make new open font
                open_font_info info;
                adapter->get_metrics(i, info.metrics);

                info.family = fam_name;
                info.idx = static_cast<std::int32_t>(i);
                info.face_attrib = attrib;
                info.adapter = adapter.get();

                open_font_store.push_back(std::move(info));
            }
        }

        font_adapters.push_back(std::move(adapter));
    }

    open_font_info *font_store::seek_the_font_by_uid(const epoc::uid the_uid) {
        for (auto &info: open_font_store) {
            if (info.adapter->unique_id(info.idx) == the_uid) {
                return &info;
            }
        }

        return nullptr;
    }

    open_font_info *font_store::seek_the_font_by_id(std::uint32_t index) {
        if (index >= open_font_store.size()) {
            return nullptr;
        }
        return &open_font_store[index];
    }
    
    open_font_info *font_store::seek_the_open_font(epoc::font_spec_base &spec) {
        open_font_info *best = nullptr;
        int best_score = -99999999;

        const std::u16string my_name = spec.tf.name.to_std_string(nullptr);

        for (auto &info : open_font_store) {
            bool maybe_same_family = (my_name.find(info.family) != std::string::npos);
            int score = 0;

            // Better returns me!
            if (info.face_attrib.name.to_std_string(nullptr) == my_name) {
                return &info;
            }

            if (maybe_same_family) {
                score += 10000;
            }

            if (!info.adapter->vectorizable()) {
                // Font that is not vectorizable, according to test, can't resize
                score -= common::abs(spec.height - info.metrics.max_height) * 100;
            }

            // Match the flags. This is also an important factor.
            if (info.face_attrib.style & epoc::open_font_face_attrib::italic) {
                // Extra flags are not welcome
                if ((static_cast<epoc::font_spec_v1&>(spec).style.flags & epoc::font_style_base::italic)) {
                    score += 5000;
                } else {
                    score -= 3000;
                }
            }

            if (info.face_attrib.style & epoc::open_font_face_attrib::bold) {    
                if (static_cast<epoc::font_spec_v1&>(spec).style.flags & epoc::font_style_base::bold) {
                    score += 5000;
                } else {
                    score -= 3000;
                }
            }

            static constexpr std::uint32_t COVERAGE_WORD_COUNT = sizeof(info.face_attrib.coverage) / sizeof(info.face_attrib.coverage[0]);

            // The more coverage, the more varieties of the font.
            for (std::size_t i = 0; i < COVERAGE_WORD_COUNT; i++) {
                score += 100 * common::count_bit_set(info.face_attrib.coverage[i]);
            }

            if (score > best_score) {
                best = &info;
                best_score = score;
            }
        }

        return best;
    }
}