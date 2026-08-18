/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#include <catch2/catch.hpp>

#include <services/fbs/adapter/linked_font_adapter.h>
#include <services/fbs/linked_font_config.h>

#include <set>

using namespace eka2l1;

// A stanza of the X7 (rm-707) ROM's Z:\Resource\Fonts\link.ini, which pairs
// each Nokia Sans face with the simplified Chinese font.
static const std::u16string SCHR_SECTION =
    u"[SCHR_LINK_START]\r\n"
    u"Nokia Sans S60 Regular:GROUP2:CANONICAL1:REGULAR:SNRegular:FNNOKSCHRSANSRLF:\r\n"
    u"MHeiM-C-GB18030-S60:GROUP1:CANONICAL0:REGULAR:SNRegular:FNNOKSCHRSANSRLF:\r\n"
    u"Nokia Sans S60 SemiBold:GROUP2:CANONICAL1:REGULAR:SNSemiBold:FNNOKSCHRSANSSBLF:\r\n"
    u"MHeiM-C-GB18030-S60:GROUP1:CANONICAL0:REGULAR:SNSemiBold:FNNOKSCHRSANSSBLF:\r\n"
    u"[SCHR_LINK_STOP]\r\n";

TEST_CASE("linked_font_config_parses_typefaces", "fbs") {
    const std::vector<epoc::linked_font_spec> specs = epoc::parse_linked_font_config(SCHR_SECTION);

    REQUIRE(specs.size() == 2);

    // The FN field names the typeface, and the typefaces keep the order the
    // file introduces them in.
    REQUIRE(specs[0].name == u"NOKSCHRSANSRLF");
    REQUIRE(specs[1].name == u"NOKSCHRSANSSBLF");

    // Components stay in file order: the Latin font is consulted first, so
    // Latin text keeps its own face and only the rest falls to the CJK font.
    REQUIRE(specs[1].component_names.size() == 2);
    REQUIRE(specs[1].component_names[0] == u"Nokia Sans S60 SemiBold");
    REQUIRE(specs[1].component_names[1] == u"MHeiM-C-GB18030-S60");

    // CANONICAL1 is on the Latin component, and the SN field must not be
    // mistaken for a typeface name (S60's own parser does exactly that).
    REQUIRE(specs[1].canonical == 0);
}

TEST_CASE("linked_font_config_ignores_unknown_sections", "fbs") {
    // Section markers are not honoured: every variant's typefaces are offered,
    // and the store drops the ones this ROM has no fonts for.
    const std::vector<epoc::linked_font_spec> specs = epoc::parse_linked_font_config(
        u"[TCHKHR_LINK_START]\r\n"
        u"Nokia Sans S60 Regular:GROUP2:CANONICAL1:REGULAR:SNRegular:FNNOKTCHKHRSANSRLF:\r\n"
        u"MHeiM-C-B5HK-S60:GROUP1:CANONICAL0:REGULAR:SNRegular:FNNOKTCHKHRSANSRLF:\r\n"
        u"[TCHKHR_LINK_STOP]\r\n");

    REQUIRE(specs.size() == 1);
    REQUIRE(specs[0].name == u"NOKTCHKHRSANSRLF");
    REQUIRE(specs[0].component_names.size() == 2);
}

TEST_CASE("linked_font_config_skips_incomplete_lines", "fbs") {
    // A line without an FN field names no typeface, and a stray blank or
    // comment-looking line is not an element either.
    const std::vector<epoc::linked_font_spec> specs = epoc::parse_linked_font_config(
        u"\r\n"
        u"Nokia Sans S60 Regular:GROUP2:CANONICAL1:\r\n"
        u"   \r\n"
        u"Nokia Sans S60 Regular:GROUP2:CANONICAL1:REGULAR:SNRegular:FNONLYONE:\r\n");

    REQUIRE(specs.size() == 1);
    REQUIRE(specs[0].name == u"ONLYONE");
    REQUIRE(specs[0].component_names.size() == 1);
}

TEST_CASE("linked_font_config_decodes_utf16_and_utf8", "fbs") {
    const std::uint8_t utf16[] = { 0xFF, 0xFE, 'F', 0x00, 'N', 0x00, 'X', 0x00 };
    REQUIRE(epoc::decode_linked_font_config(utf16, sizeof(utf16)) == u"FNX");

    const std::uint8_t utf8[] = { 'F', 'N', 'X' };
    REQUIRE(epoc::decode_linked_font_config(utf8, sizeof(utf8)) == u"FNX");
}

namespace {
    // Stands in for a real font file: knows a fixed set of codepoints and hands
    // out a bitmap it owns, so bitmap ownership can be checked.
    struct fake_font_adapter : public epoc::adapter::font_file_adapter_base {
        std::set<std::int32_t> codepoints_;
        std::uint32_t advance_;
        std::size_t live_bitmaps_ = 0;

        explicit fake_font_adapter(std::set<std::int32_t> codepoints, const std::uint32_t advance)
            : codepoints_(std::move(codepoints))
            , advance_(advance) {
        }

        bool is_valid() override {
            return true;
        }

        bool vectorizable() const override {
            return true;
        }

        std::size_t count() override {
            return 1;
        }

        bool get_face_attrib(const std::size_t idx, epoc::open_font_face_attrib &face_attrib) override {
            face_attrib = {};
            return true;
        }

        epoc::glyph_bitmap_type requested_bitmap_type_ = epoc::default_glyph_bitmap;

        std::uint8_t *get_glyph_bitmap(const std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier,
            int *rasterized_width, int *rasterized_height, std::uint32_t &total_size, epoc::glyph_bitmap_type *bmp_type,
            epoc::open_font_character_metric &character_metric) override {
            live_bitmaps_++;
            total_size = 1;

            if (bmp_type) {
                requested_bitmap_type_ = *bmp_type;
                *bmp_type = epoc::monochrome_glyph_bitmap;
            }

            if (rasterized_width) {
                *rasterized_width = 1;
            }

            if (rasterized_height) {
                *rasterized_height = 1;
            }

            return new std::uint8_t(static_cast<std::uint8_t>(advance_));
        }

        void free_glyph_bitmap(std::uint8_t *data) override {
            live_bitmaps_--;
            delete data;
        }

        epoc::glyph_bitmap_type get_output_bitmap_type() const override {
            return epoc::monochrome_glyph_bitmap;
        }

        bool does_glyph_exist(std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier) override {
            return codepoints_.count(static_cast<std::int32_t>(code)) != 0;
        }

        bool has_character(const std::size_t face_index, const std::int32_t codepoint, const std::uint32_t metric_identifier) override {
            return codepoints_.count(codepoint) != 0;
        }

        std::uint32_t last_metric_identifier_ = 0xFFFFFFFF;

        std::uint32_t get_glyph_advance(const std::size_t face_index, const std::uint32_t codepoint,
            const std::uint32_t metric_identifier, const bool vertical) override {
            last_metric_identifier_ = metric_identifier;
            return advance_;
        }

        std::int32_t begin_get_atlas(std::uint8_t *atlas_ptr, const eka2l1::vec2 atlas_size) override {
            return 0;
        }

        bool get_glyph_atlas(const std::int32_t handle, const std::size_t idx, const char16_t start_code, int *unicode_point,
            const char16_t num_code, const std::uint32_t metric_identifier, epoc::adapter::character_info *info) override {
            return true;
        }

        void end_get_atlas(const std::int32_t handle) override {
        }

        std::optional<epoc::open_font_metrics> get_metric_with_uid(const std::size_t face_index, const std::uint32_t uid,
            std::uint32_t *metric_identifier) override {
            return std::nullopt;
        }

        std::optional<epoc::open_font_metrics> get_nearest_supported_metric(const std::size_t face_index,
            const std::uint16_t targeted_font_size, std::uint32_t *metric_identifier, bool is_design_font_size) override {
            epoc::open_font_metrics metrics{};
            metrics.design_height = static_cast<std::int16_t>(targeted_font_size);

            if (metric_identifier) {
                *metric_identifier = targeted_font_size;
            }

            return metrics;
        }
    };

    // A component whose metric identifier means something else entirely: gdr
    // numbers a face's bitmaps, while FreeType uses the pixel size.
    struct indexed_font_adapter : public fake_font_adapter {
        using fake_font_adapter::fake_font_adapter;

        std::optional<epoc::open_font_metrics> get_nearest_supported_metric(const std::size_t face_index,
            const std::uint16_t targeted_font_size, std::uint32_t *metric_identifier, bool is_design_font_size) override {
            epoc::open_font_metrics metrics{};
            metrics.design_height = static_cast<std::int16_t>(targeted_font_size);

            // Whatever the size, this face only has two bitmaps.
            if (metric_identifier) {
                *metric_identifier = (targeted_font_size > 12) ? 1 : 0;
            }

            return metrics;
        }

    };

    constexpr std::int32_t LATIN_CODE = u'A';
    constexpr std::int32_t CJK_CODE = 0x6587; // 文
    constexpr std::int32_t UNCOVERED_CODE = 0x1F600;
}

TEST_CASE("linked_font_adapter_dispatches_per_glyph", "fbs") {
    fake_font_adapter latin({ LATIN_CODE }, 7);
    fake_font_adapter cjk({ LATIN_CODE, CJK_CODE }, 13);

    epoc::open_font_face_attrib attrib{};
    attrib.style = epoc::open_font_face_attrib::bold;

    // Latin first, as link.ini lists it, and canonical.
    epoc::adapter::linked_font_file_adapter linked({ { &latin, 0 }, { &cjk, 0 } }, 0, attrib);

    // A codepoint both cover comes from the first component, so Latin text is
    // not silently restyled by the CJK font.
    REQUIRE(linked.get_glyph_advance(0, LATIN_CODE, 12, false) == 7);

    // One only the CJK font has goes there instead of rendering .notdef.
    REQUIRE(linked.get_glyph_advance(0, CJK_CODE, 12, false) == 13);

    // Nothing covers this one; the canonical component answers, as it would
    // have without linking.
    REQUIRE(linked.get_glyph_advance(0, UNCOVERED_CODE, 12, false) == 7);

    // The typeface covers the union of its components.
    REQUIRE(linked.has_character(0, LATIN_CODE, 12));
    REQUIRE(linked.has_character(0, CJK_CODE, 12));
    REQUIRE(!linked.has_character(0, UNCOVERED_CODE, 12));

    // It presents itself as one face, wearing the canonical attributes.
    REQUIRE(linked.count() == 1);

    epoc::open_font_face_attrib reported{};
    REQUIRE(linked.get_face_attrib(0, reported));
    REQUIRE(reported.style == epoc::open_font_face_attrib::bold);
    REQUIRE(!linked.get_face_attrib(1, reported));
}

TEST_CASE("linked_font_adapter_sends_a_glyph_index_to_the_canonical_font", "fbs") {
    // Shaping hands back glyph indices rather than codepoints, flagged with the
    // high bit, and an index only means anything to the face that produced it --
    // which is the canonical component, the one that did the shaping. Routing an
    // index by "codepoint" lands it in whichever component claims that number
    // and draws an unrelated glyph.
    fake_font_adapter latin({ LATIN_CODE, static_cast<std::int32_t>(0x80000000 | LATIN_CODE) }, 7);
    fake_font_adapter cjk({ CJK_CODE }, 13);

    // The CJK font is canonical here, so a misrouted index is visible.
    epoc::adapter::linked_font_file_adapter linked({ { &latin, 0 }, { &cjk, 0 } }, 1, {});

    REQUIRE(linked.get_glyph_advance(0, 0x80000000 | LATIN_CODE, 12, false) == 13);

    // The same number without the flag is a codepoint, and does go to Latin.
    REQUIRE(linked.get_glyph_advance(0, LATIN_CODE, 12, false) == 7);
}

TEST_CASE("linked_font_adapter_keeps_the_canonical_font_in_charge", "fbs") {
    // link.ini marks one component canonical, and that is the face the linked
    // typeface presents as -- its attributes and its metrics. The others are
    // reachable only for glyphs the canonical one does not have, so nothing
    // about the device font's look changes.
    fake_font_adapter device({ LATIN_CODE }, 7);
    fake_font_adapter cjk({ CJK_CODE }, 13);

    epoc::open_font_face_attrib attrib{};
    attrib.style = epoc::open_font_face_attrib::serif;

    epoc::adapter::linked_font_file_adapter linked({ { &device, 0 }, { &cjk, 0 } }, 0, attrib);

    std::uint32_t metric_identifier = 0;
    const std::optional<epoc::open_font_metrics> metrics = linked.get_nearest_supported_metric(0, 17, &metric_identifier, true);

    REQUIRE(metrics.has_value());
    REQUIRE(metrics->design_height == 17);
    REQUIRE(metric_identifier == 17);

    epoc::open_font_face_attrib reported{};
    REQUIRE(linked.get_face_attrib(0, reported));
    REQUIRE(reported.style == epoc::open_font_face_attrib::serif);

    // Only what the device face cannot draw reaches the CJK one.
    REQUIRE(linked.get_glyph_advance(0, LATIN_CODE, 17, false) == 7);
    REQUIRE(linked.get_glyph_advance(0, CJK_CODE, 17, false) == 13);
}

TEST_CASE("monochrome_glyph_compression_repeats_identical_scanlines", "fbs") {
    // Symbian's monochrome glyph: a mode bit -- 0 for "the one scanline that
    // follows repeats" -- then a four bit count, then the scanline, all least
    // significant bit first. Two identical four pixel rows cost one scanline.
    const std::uint32_t source[] = { 0b1010'1010 };

    std::vector<std::uint32_t> dest(epoc::adapter::monochrome_glyph_word_count(4, 2), 0);
    const std::uint32_t written = epoc::adapter::compress_monochrome_glyph(source, 4, 2, dest.data());

    REQUIRE(written == 1 + 4 + 4);
    REQUIRE((dest[0] & 1) == 0); // repeat mode
    REQUIRE(((dest[0] >> 1) & 0xF) == 2); // twice
    REQUIRE(((dest[0] >> 5) & 0xF) == 0b1010);
}

TEST_CASE("monochrome_glyph_compression_writes_differing_scanlines_out", "fbs") {
    // Two rows that differ have to be written verbatim, so both are in there.
    const std::uint32_t source[] = { 0b0101'1010 };

    std::vector<std::uint32_t> dest(epoc::adapter::monochrome_glyph_word_count(4, 2), 0);
    const std::uint32_t written = epoc::adapter::compress_monochrome_glyph(source, 4, 2, dest.data());

    REQUIRE(written == 1 + 4 + 8);
    REQUIRE((dest[0] & 1) == 1); // verbatim mode
    REQUIRE(((dest[0] >> 1) & 0xF) == 2); // two scanlines
    REQUIRE(((dest[0] >> 5) & 0xFF) == 0b0101'1010);
}

TEST_CASE("linked_font_glyphs_are_presentable_only_where_convertible", "fbs") {
    // A component backing a face has to be able to hand over glyphs in the
    // format that face declares. Antialiased output can be thresholded down to
    // monochrome; the other direction is not implemented.
    REQUIRE(epoc::adapter::can_present_glyph_as(epoc::antialised_glyph_bitmap, epoc::antialised_glyph_bitmap));
    REQUIRE(epoc::adapter::can_present_glyph_as(epoc::monochrome_glyph_bitmap, epoc::monochrome_glyph_bitmap));
    REQUIRE(epoc::adapter::can_present_glyph_as(epoc::antialised_glyph_bitmap, epoc::monochrome_glyph_bitmap));
    REQUIRE(!epoc::adapter::can_present_glyph_as(epoc::monochrome_glyph_bitmap, epoc::antialised_glyph_bitmap));
}

TEST_CASE("font_adapters_reject_what_they_cannot_read", "fbs") {
    // Loading a font by content depends on this: a file is handed to each
    // rasterizer in turn and the one that recognises it keeps it, which only
    // works if the others say no. The 5320 keeps its Chinese font in a file
    // named s60sc.ccc, so the extension cannot be trusted.
    std::vector<std::uint8_t> garbage(256, 0x5A);

    for (const auto kind : { epoc::adapter::font_file_adapter_kind::freetype,
             epoc::adapter::font_file_adapter_kind::gdr }) {
        auto adapter = epoc::adapter::make_font_file_adapter(kind, garbage);

        REQUIRE(adapter);
        REQUIRE(!adapter->is_valid());
    }
}

TEST_CASE("linked_font_adapter_translates_metric_identifiers", "fbs") {
    // A metric identifier is only meaningful to the adapter that issued it, and
    // the per-glyph calls only ever carry the canonical's. Handing a gdr bitmap
    // index to FreeType as a pixel size renders the glyph a couple of pixels
    // tall, which is what a device pairing a gdr Latin face with a scalable CJK
    // one would draw.
    indexed_font_adapter device({ LATIN_CODE }, 7);
    fake_font_adapter cjk({ CJK_CODE }, 13);

    epoc::adapter::linked_font_file_adapter linked({ { &device, 0 }, { &cjk, 0 } }, 0, {});

    std::uint32_t metric_identifier = 0;
    REQUIRE(linked.get_nearest_supported_metric(0, 17, &metric_identifier, true).has_value());

    // The canonical's own numbering is what the caller is given back.
    REQUIRE(metric_identifier == 1);

    // The canonical still sees its own identifier...
    REQUIRE(linked.get_glyph_advance(0, LATIN_CODE, metric_identifier, false) == 7);
    REQUIRE(device.last_metric_identifier_ == 1);

    // ...while the other component is addressed the way it expects, by size.
    REQUIRE(linked.get_glyph_advance(0, CJK_CODE, metric_identifier, false) == 13);
    REQUIRE(cjk.last_metric_identifier_ == 17);
}

TEST_CASE("linked_font_adapter_asks_components_for_the_face_format", "fbs") {
    // Rather than convert after the fact, the component is told which format
    // the face declares -- FreeType can rasterise monochrome itself, hinted,
    // which is far kinder to a small CJK glyph than thresholding a grey one.
    fake_font_adapter device({ LATIN_CODE }, 7);
    fake_font_adapter cjk({ CJK_CODE }, 13);

    epoc::adapter::linked_font_file_adapter linked({ { &device, 0 }, { &cjk, 0 } }, 0, {});

    int width = 0;
    int height = 0;
    std::uint32_t total_size = 0;
    epoc::glyph_bitmap_type bmp_type = epoc::default_glyph_bitmap;
    epoc::open_font_character_metric metric{};

    std::uint8_t *bitmap = linked.get_glyph_bitmap(0, CJK_CODE, 12, &width, &height, total_size, &bmp_type, metric);

    REQUIRE(bitmap);
    REQUIRE(cjk.requested_bitmap_type_ == epoc::monochrome_glyph_bitmap);
    REQUIRE(bmp_type == epoc::monochrome_glyph_bitmap);

    linked.free_glyph_bitmap(bitmap);
    REQUIRE(cjk.live_bitmaps_ == 0);
}

TEST_CASE("linked_font_adapter_frees_bitmap_through_its_owner", "fbs") {
    fake_font_adapter latin({ LATIN_CODE }, 7);
    fake_font_adapter cjk({ CJK_CODE }, 13);

    epoc::adapter::linked_font_file_adapter linked({ { &latin, 0 }, { &cjk, 0 } }, 0, {});

    int width = 0;
    int height = 0;
    std::uint32_t total_size = 0;
    epoc::glyph_bitmap_type bmp_type = epoc::default_glyph_bitmap;
    epoc::open_font_character_metric metric{};

    // free_glyph_bitmap() only gets a pointer back, so the adapter has to
    // remember which component allocated it -- gdr and stb really do own it.
    std::uint8_t *cjk_bitmap = linked.get_glyph_bitmap(0, CJK_CODE, 12, &width, &height, total_size, &bmp_type, metric);

    REQUIRE(cjk_bitmap);
    REQUIRE(cjk.live_bitmaps_ == 1);
    REQUIRE(latin.live_bitmaps_ == 0);

    linked.free_glyph_bitmap(cjk_bitmap);

    REQUIRE(cjk.live_bitmaps_ == 0);
    REQUIRE(latin.live_bitmaps_ == 0);
}
