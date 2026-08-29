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

#include <services/fbs/adapter/linked_font_adapter.h>

#include <vector>

namespace eka2l1::epoc::adapter {
    bool can_present_glyph_as(const glyph_bitmap_type component_type, const glyph_bitmap_type face_type) {
        return (component_type == face_type)
            || ((face_type == monochrome_glyph_bitmap) && (component_type == antialised_glyph_bitmap));
    }

    // Threshold an 8 bit per pixel glyph and encode it the way a monochrome
    // face's own glyphs arrive, so a face whose device only understands those
    // can still be backed by an imported TrueType font.
    static std::uint8_t *to_monochrome_glyph(const std::uint8_t *gray, const std::int32_t width,
        const std::int32_t height, std::uint32_t &total_size) {
        std::vector<std::uint32_t> bits((static_cast<std::size_t>(width) * height + 31) >> 5, 0);

        for (std::int32_t i = 0; i < width * height; i++) {
            if (gray[i] >= 0x80) {
                bits[i >> 5] |= (1u << (i & 31));
            }
        }

        const std::size_t word_count = monochrome_glyph_word_count(width, height);
        std::uint32_t *compressed = new std::uint32_t[word_count];
        std::fill(compressed, compressed + word_count, 0);

        const std::uint32_t written = compress_monochrome_glyph(bits.data(), width, height, compressed);
        total_size = ((written + 31) >> 5) * 4;

        return reinterpret_cast<std::uint8_t *>(compressed);
    }
    linked_font_file_adapter::linked_font_file_adapter(std::vector<component> components, const std::size_t canonical,
        const open_font_face_attrib &attrib)
        : components_(std::move(components))
        , canonical_(canonical)
        , attrib_(attrib) {
    }

    std::uint32_t linked_font_file_adapter::metric_for(const std::size_t component_index,
        const std::uint32_t metric_identifier) const {
        auto translated = metric_identifiers_.find(metric_identifier);

        if ((translated == metric_identifiers_.end()) || (component_index >= translated->second.size())) {
            return metric_identifier;
        }

        return translated->second[component_index];
    }

    std::size_t linked_font_file_adapter::component_index_for(const std::uint32_t code,
        const std::uint32_t metric_identifier) {
        // A code with the high bit set is already a glyph index, which only
        // means anything to the face it was taken from. Those come back from
        // shaping, which the canonical component performs.
        if (!(code & 0x80000000)) {
            for (std::size_t i = 0; i < components_.size(); i++) {
                if (components_[i].adapter_->has_character(components_[i].face_index_, static_cast<std::int32_t>(code),
                        metric_for(i, metric_identifier))) {
                    return i;
                }
            }
        }

        return canonical_;
    }

    bool linked_font_file_adapter::is_valid() {
        return !components_.empty();
    }

    bool linked_font_file_adapter::vectorizable() const {
        return components_[canonical_].adapter_->vectorizable();
    }

    std::size_t linked_font_file_adapter::count() {
        return 1;
    }

    std::uint32_t linked_font_file_adapter::line_gap(const std::size_t idx, const std::uint32_t metric_identifier) {
        return canonical().adapter_->line_gap(canonical().face_index_, metric_identifier);
    }

    bool linked_font_file_adapter::get_face_attrib(const std::size_t idx, open_font_face_attrib &face_attrib) {
        if (idx != 0) {
            return false;
        }

        face_attrib = attrib_;
        return true;
    }

    std::uint8_t *linked_font_file_adapter::get_glyph_bitmap(const std::size_t idx, std::uint32_t code,
        const std::uint32_t metric_identifier, int *rasterized_width, int *rasterized_height, std::uint32_t &total_size,
        epoc::glyph_bitmap_type *bmp_type, open_font_character_metric &character_metric) {
        const std::size_t index = component_index_for(code, metric_identifier);
        const component &comp = components_[index];

        int width = 0;
        int height = 0;

        // Ask for the format the face declares. A component that can render it
        // -- FreeType has its own monochrome rasteriser, and a hinted glyph
        // beats a thresholded one at the sizes these devices use -- gives it
        // back directly.
        glyph_bitmap_type produced = get_output_bitmap_type();

        std::uint8_t *result = comp.adapter_->get_glyph_bitmap(comp.face_index_, code,
            metric_for(index, metric_identifier), &width, &height, total_size, &produced, character_metric);

        if (rasterized_width) {
            *rasterized_width = width;
        }

        if (rasterized_height) {
            *rasterized_height = height;
        }

        if (!result) {
            return nullptr;
        }

        // A component that ignored the request has to be converted instead:
        // the guest reads a glyph in the format the font as a whole declares,
        // and 8 bit per pixel data read as monochrome runs draws as streaks.
        if ((produced != get_output_bitmap_type()) && (width > 0) && (height > 0)) {
            std::uint8_t *converted = to_monochrome_glyph(result, width, height, total_size);
            comp.adapter_->free_glyph_bitmap(result);

            produced = monochrome_glyph_bitmap;
            character_metric.bitmap_type = produced;
            result = converted;

            bitmap_owners_[result] = OWNED_BY_LINK;
        } else {
            bitmap_owners_[result] = index;
        }

        if (bmp_type) {
            *bmp_type = produced;
        }

        return result;
    }

    void linked_font_file_adapter::free_glyph_bitmap(std::uint8_t *data) {
        auto owner = bitmap_owners_.find(data);

        if (owner == bitmap_owners_.end()) {
            canonical().adapter_->free_glyph_bitmap(data);
            return;
        }

        const std::size_t index = owner->second;
        bitmap_owners_.erase(owner);

        if (index == OWNED_BY_LINK) {
            delete[] reinterpret_cast<std::uint32_t *>(data);
            return;
        }

        components_[index].adapter_->free_glyph_bitmap(data);
    }

    glyph_bitmap_type linked_font_file_adapter::get_output_bitmap_type() const {
        return components_[canonical_].adapter_->get_output_bitmap_type();
    }

    bool linked_font_file_adapter::does_glyph_exist(std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier) {
        const std::size_t index = component_index_for(code, metric_identifier);
        return components_[index].adapter_->does_glyph_exist(components_[index].face_index_, code,
            metric_for(index, metric_identifier));
    }

    bool linked_font_file_adapter::has_character(const std::size_t face_index, const std::int32_t codepoint,
        const std::uint32_t metric_identifier) {
        for (std::size_t i = 0; i < components_.size(); i++) {
            if (components_[i].adapter_->has_character(components_[i].face_index_, codepoint,
                    metric_for(i, metric_identifier))) {
                return true;
            }
        }

        return false;
    }

    std::uint32_t linked_font_file_adapter::get_glyph_advance(const std::size_t face_index, const std::uint32_t codepoint,
        const std::uint32_t metric_identifier, const bool vertical) {
        const std::size_t index = component_index_for(codepoint, metric_identifier);
        return components_[index].adapter_->get_glyph_advance(components_[index].face_index_, codepoint,
            metric_for(index, metric_identifier), vertical);
    }

    bool linked_font_file_adapter::can_share_atlas(const std::size_t component_index) {
        // Every component draws into the one buffer the caller uploads as a
        // single texture, in whatever format it writes. Only those writing the
        // format the atlas was made in -- the canonical's -- can take part; a
        // gdr face backed by an imported TrueType font is the case that cares.
        return components_[component_index].adapter_->get_atlas_bitmap_bits_per_pixel()
            == components_[canonical_].adapter_->get_atlas_bitmap_bits_per_pixel();
    }

    // Sort `codes` by the component that draws them and run `handler` once per
    // component, so a component still sees its glyphs as one batch. `slots`
    // maps each entry of the group back to its index in the caller's arrays.
    template <typename F>
    bool linked_font_file_adapter::for_each_component_group(const int *codes, const std::size_t count,
        const std::uint32_t metric_identifier, F handler) {
        std::map<std::size_t, std::pair<std::vector<int>, std::vector<std::size_t>>> groups;

        for (std::size_t i = 0; i < count; i++) {
            const std::size_t chosen = component_index_for(static_cast<std::uint32_t>(codes[i]), metric_identifier);

            auto &group = groups[can_share_atlas(chosen) ? chosen : canonical_];

            group.first.push_back(codes[i]);
            group.second.push_back(i);
        }

        for (auto &group : groups) {
            if (!handler(components_[group.first], group.first, group.second.first, group.second.second)) {
                return false;
            }
        }

        return true;
    }

    // The atlas belongs to the caller, which packs it and tells each component
    // where to draw, so these two only have to route a codepoint to the
    // component that can draw it -- exactly as get_glyph_bitmap() does.
    bool linked_font_file_adapter::measure_atlas_glyphs(const std::size_t idx, const int *codes, const std::size_t count,
        const std::uint32_t metric_identifier, eka2l1::vec2 *sizes) {
        return for_each_component_group(codes, count, metric_identifier,
            [&](const component &comp, const std::size_t component_index, const std::vector<int> &group,
                const std::vector<std::size_t> &slots) {
                std::vector<eka2l1::vec2> measured(group.size());

                if (!comp.adapter_->measure_atlas_glyphs(comp.face_index_, group.data(), group.size(),
                        metric_for(component_index, metric_identifier), measured.data())) {
                    return false;
                }

                for (std::size_t i = 0; i < group.size(); i++) {
                    sizes[slots[i]] = measured[i];
                }

                return true;
            });
    }

    bool linked_font_file_adapter::render_atlas_glyphs(const std::size_t idx, const int *codes, const std::size_t count,
        const std::uint32_t metric_identifier, std::uint8_t *atlas, const eka2l1::vec2 atlas_size,
        const eka2l1::vec2 *positions, character_info *info) {
        return for_each_component_group(codes, count, metric_identifier,
            [&](const component &comp, const std::size_t component_index, const std::vector<int> &group,
                const std::vector<std::size_t> &slots) {
                std::vector<eka2l1::vec2> where(group.size());
                std::vector<character_info> drawn(group.size());

                for (std::size_t i = 0; i < group.size(); i++) {
                    where[i] = positions[slots[i]];
                }

                if (!comp.adapter_->render_atlas_glyphs(comp.face_index_, group.data(), group.size(),
                        metric_for(component_index, metric_identifier), atlas, atlas_size, where.data(),
                        drawn.data())) {
                    return false;
                }

                for (std::size_t i = 0; i < group.size(); i++) {
                    info[slots[i]] = drawn[i];
                }

                return true;
            });
    }

    std::uint8_t linked_font_file_adapter::get_atlas_bitmap_bits_per_pixel() {
        return canonical().adapter_->get_atlas_bitmap_bits_per_pixel();
    }

    std::optional<open_font_metrics> linked_font_file_adapter::get_metric_with_uid(const std::size_t face_index,
        const std::uint32_t uid, std::uint32_t *metric_identifier) {
        return canonical().adapter_->get_metric_with_uid(canonical().face_index_, uid, metric_identifier);
    }

    std::optional<open_font_metrics> linked_font_file_adapter::get_nearest_supported_metric(const std::size_t face_index,
        const std::uint16_t targeted_font_size, std::uint32_t *metric_identifier, bool is_design_font_size) {
        std::uint32_t canonical_identifier = 0;
        std::optional<open_font_metrics> metrics = canonical().adapter_->get_nearest_supported_metric(
            canonical().face_index_, targeted_font_size, &canonical_identifier, is_design_font_size);

        if (!metrics.has_value()) {
            return metrics;
        }

        // Ask every component how it addresses this same size, while the size
        // is still to hand: the per-glyph calls only ever see the canonical's
        // identifier, and handing a gdr bitmap index to FreeType as a pixel
        // size renders the glyph at a couple of pixels tall.
        //
        // What they are asked for is the size the canonical actually settled
        // on, not the one the client asked for. A linked typeface has to look
        // like one face, and a bitmap canonical only stocks a few sizes, so it
        // routinely lands somewhere other than the request. Where the request
        // is itself unusable -- S60v2 clients have been seen asking for two
        // pixels while the canonical renders at thirteen -- passing it on
        // shrinks every fallback glyph to nothing.
        const std::uint16_t settled_size = (metrics->max_height > 0)
            ? static_cast<std::uint16_t>(metrics->max_height)
            : targeted_font_size;

        std::vector<std::uint32_t> &identifiers = metric_identifiers_[canonical_identifier];
        identifiers.assign(components_.size(), canonical_identifier);

        for (std::size_t i = 0; i < components_.size(); i++) {
            if (i == canonical_) {
                continue;
            }

            std::uint32_t component_identifier = canonical_identifier;

            if (components_[i].adapter_->get_nearest_supported_metric(components_[i].face_index_, settled_size,
                    &component_identifier, false)) {
                identifiers[i] = component_identifier;
            }
        }

        if (metric_identifier) {
            *metric_identifier = canonical_identifier;
        }

        return metrics;
    }

    bool linked_font_file_adapter::get_table_content(const std::size_t face_index, const std::uint32_t tag4,
        std::uint8_t *dest, std::uint32_t &dest_size) {
        return canonical().adapter_->get_table_content(canonical().face_index_, tag4, dest, dest_size);
    }

}
