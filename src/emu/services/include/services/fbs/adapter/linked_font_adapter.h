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

#pragma once

#include <services/fbs/adapter/font_adapter.h>

#include <map>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace eka2l1::epoc::adapter {
    /**
     * @brief A typeface assembled from faces belonging to other font files.
     *
     * This is the S60 linked font: a Chinese/Japanese/Korean variant ships a
     * Latin typeface and a CJK typeface as separate files, plus a link.ini
     * describing how to present them as one typeface. A glyph is taken from
     * the first component that has it, so Latin text keeps the look of the
     * Latin font while CJK text -- absent from it -- comes from the CJK font.
     *
     * The component flagged CANONICAL in link.ini supplies everything that is
     * not per-glyph: the typeface attributes, the metrics and the supported
     * sizes. Anything reached through a character code is dispatched instead.
     */
    /**
     * @brief Can a glyph a component produces be handed over as one from the
     *        face it is backing?
     *
     * The same format always can. Antialiased output can be converted down to
     * the monochrome one Symbian's older devices are limited to; the other way
     * around is not implemented, and no imported font asks for it.
     */
    bool can_present_glyph_as(const glyph_bitmap_type component_type, const glyph_bitmap_type face_type);

    class linked_font_file_adapter : public font_file_adapter_base {
    public:
        struct component {
            font_file_adapter_base *adapter_;
            std::size_t face_index_;

            // Zero sizes the component the way link.ini components are: asked
            // for the same maximum height as the canonical face. Otherwise it
            // is sized so that its capitals are this many percent as tall as
            // the canonical's -- for a font standing in for the canonical's
            // own glyphs, whose design may sit very differently in its box.
            int cap_height_scale_ = 0;
        };

    private:
        // In link.ini order, which is also the order glyphs are looked up in.
        std::vector<component> components_;
        std::size_t canonical_;
        open_font_face_attrib attrib_;

        // Glyph bitmaps must be released through the component that produced
        // them: free_glyph_bitmap() only gets the pointer back, and the gdr and
        // stb adapters really do own that memory. A bitmap this adapter
        // converted itself is marked with OWNED_BY_LINK.
        static constexpr std::size_t OWNED_BY_LINK = static_cast<std::size_t>(-1);
        std::unordered_map<std::uint8_t *, std::size_t> bitmap_owners_;

        // A metric identifier only means something to the adapter that issued
        // it -- a pixel size to FreeType, an index into the face's bitmaps to
        // gdr -- so the canonical's identifier is remembered alongside the one
        // each component uses for that same size. Filled in by
        // get_nearest_supported_metric, which is where the size is still known,
        // and by metric_for for a size it never saw (see there).
        mutable std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> metric_identifiers_;
        mutable std::mutex identifiers_lock_;

        // Is any component sized by capital height?
        bool has_scaled_component_ = false;

        const component &canonical() const {
            return components_[canonical_];
        }

        // How `component_index` addresses the size the canonical calls
        // `metric_identifier`.
        std::uint32_t metric_for(const std::size_t component_index, const std::uint32_t metric_identifier) const;

        // Every component's identifier for the canonical's `canonical_identifier`,
        // whose maximum height is `settled_size`. Components that are not sized
        // by capital height are only asked when `ask_unscaled` is set, and keep
        // the canonical's identifier otherwise.
        std::vector<std::uint32_t> identifiers_for(const std::uint32_t canonical_identifier,
            const std::uint16_t settled_size, const bool ask_unscaled) const;

        // Height in pixels of the component's "H" at that metric, or 0 if it
        // has none.
        static std::int32_t capital_height(const component &comp, const std::uint32_t metric_identifier);

        // The metric identifier at which `comp` draws capitals closest to
        // `target_height` pixels tall, starting from a maximum height of
        // `settled_size`.
        static std::optional<std::uint32_t> metric_for_capital_height(const component &comp,
            const std::int32_t target_height, const std::uint16_t settled_size);

        // The component that renders `code`, falling back to the canonical one
        // when no component covers it (so the caller still gets .notdef rather
        // than nothing).
        std::size_t component_index_for(const std::uint32_t code, const std::uint32_t metric_identifier);

        // Can this component draw into an atlas made for the canonical one?
        // Only if it writes the same pixel format.
        bool can_share_atlas(const std::size_t component_index);

        // Failing that, can its antialiased glyph bitmaps be copied into the
        // canonical's atlas as they are?
        bool can_draw_grey_into_atlas(const std::size_t component_index);

        // Run `handler(bitmap, width, height, metric)` on the component's
        // antialiased glyph for `code`, the bitmap being null if it has none.
        template <typename F>
        static void with_grey_glyph(const component &comp, const int code, const std::uint32_t metric_identifier,
            F handler);

        // Group `codes` by the component that draws each of them, then hand
        // every group to `handler` as one batch, along with where each entry
        // came from in the caller's arrays.
        template <typename F>
        bool for_each_component_group(const int *codes, const std::size_t count,
            const std::uint32_t metric_identifier, F handler);

    public:
        explicit linked_font_file_adapter(std::vector<component> components, const std::size_t canonical,
            const open_font_face_attrib &attrib);

        bool is_valid() override;
        bool vectorizable() const override;
        std::size_t count() override;

        std::uint32_t line_gap(const std::size_t idx, const std::uint32_t metric_identifier) override;
        bool get_face_attrib(const std::size_t idx, open_font_face_attrib &face_attrib) override;

        std::uint8_t *get_glyph_bitmap(const std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier,
            int *rasterized_width, int *rasterized_height, std::uint32_t &total_size, epoc::glyph_bitmap_type *bmp_type,
            open_font_character_metric &character_metric) override;

        void free_glyph_bitmap(std::uint8_t *data) override;
        glyph_bitmap_type get_output_bitmap_type() const override;

        bool does_glyph_exist(std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier) override;
        bool has_character(const std::size_t face_index, const std::int32_t codepoint, const std::uint32_t metric_identifier) override;

        std::uint32_t get_glyph_advance(const std::size_t face_index, const std::uint32_t codepoint,
            const std::uint32_t metric_identifier, const bool vertical = false) override;

        bool measure_atlas_glyphs(const std::size_t idx, const int *codes, const std::size_t count,
            const std::uint32_t metric_identifier, eka2l1::vec2 *sizes) override;
        bool render_atlas_glyphs(const std::size_t idx, const int *codes, const std::size_t count,
            const std::uint32_t metric_identifier, std::uint8_t *atlas, const eka2l1::vec2 atlas_size,
            const eka2l1::vec2 *positions, character_info *info) override;
        std::uint8_t get_atlas_bitmap_bits_per_pixel() override;

        std::optional<open_font_metrics> get_metric_with_uid(const std::size_t face_index, const std::uint32_t uid,
            std::uint32_t *metric_identifier) override;
        std::optional<open_font_metrics> get_nearest_supported_metric(const std::size_t face_index,
            const std::uint16_t targeted_font_size, std::uint32_t *metric_identifier, bool is_design_font_size) override;

        bool get_table_content(const std::size_t face_index, const std::uint32_t tag4, std::uint8_t *dest,
            std::uint32_t &dest_size) override;

        // make_text_shape() is deliberately not overridden: the base walks the
        // text calling get_glyph_advance(), so inheriting it gives shaping the
        // per-component advances rather than the canonical font's .notdef.
    };
}
