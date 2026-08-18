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

#include <services/fbs/font_store.h>

using namespace eka2l1;

namespace {
    epoc::font_spec_v1 make_spec(const std::u16string &name, const std::int32_t height, const std::uint32_t style_flags,
        const std::uint32_t typeface_flags = epoc::typeface_info::tf_propotional) {
        epoc::font_spec_v1 spec{};

        spec.tf.name.assign(nullptr, name);
        spec.tf.flags = typeface_flags;
        spec.height = height;
        spec.style.flags = style_flags;

        return spec;
    }

    epoc::open_font_face_attrib make_face(const std::u16string &name, const std::int32_t style) {
        epoc::open_font_face_attrib attrib{};

        attrib.name.assign(nullptr, name);
        attrib.fam_name.assign(nullptr, name);
        attrib.style = style;

        return attrib;
    }
}

TEST_CASE("font_match_prefers_name_over_every_style_attribute", "fbs") {
    // MatchFontSpecsInPixels pays 10 for the name and 2 for the weight, so a
    // font asked for by name wins even when another one matches the weight.
    epoc::font_spec_v1 spec = make_spec(u"MHeiM-C-GB18030-S60", 20, epoc::font_style_base::bold);

    const epoc::open_font_face_attrib named = make_face(u"MHeiM-C-GB18030-S60", 0);
    const epoc::open_font_face_attrib bold = make_face(u"Nokia Sans S60 SemiBold", epoc::open_font_face_attrib::bold);

    REQUIRE(epoc::match_font_spec(named, u"MHeiM-C-GB18030-S60", spec, 20)
        > epoc::match_font_spec(bold, u"Nokia Sans S60 SemiBold", spec, 20));
}

TEST_CASE("font_match_costs_more_for_height_than_for_weight", "fbs") {
    // Height is worth 10 - |diff|, so being two pixels off already outweighs
    // the 2 points a weight match is worth. The old scoring had bold override
    // up to fifty pixels of difference.
    epoc::font_spec_v1 spec = make_spec(u"", 20, epoc::font_style_base::bold);

    const epoc::open_font_face_attrib bold = make_face(u"Bitmap Bold", epoc::open_font_face_attrib::bold);
    const epoc::open_font_face_attrib regular = make_face(u"Bitmap Regular", 0);

    REQUIRE(epoc::match_font_spec(regular, u"Bitmap Regular", spec, 20)
        > epoc::match_font_spec(bold, u"Bitmap Bold", spec, 17));

    // At the same height the weight decides after all.
    REQUIRE(epoc::match_font_spec(bold, u"Bitmap Bold", spec, 20)
        > epoc::match_font_spec(regular, u"Bitmap Regular", spec, 20));
}

TEST_CASE("font_match_scores_italic_mono_and_serif", "fbs") {
    epoc::font_spec_v1 upright = make_spec(u"", 20, 0);
    epoc::font_spec_v1 italic = make_spec(u"", 20, epoc::font_style_base::italic);

    const epoc::open_font_face_attrib slanted = make_face(u"Face Italic", epoc::open_font_face_attrib::italic);
    const epoc::open_font_face_attrib plain = make_face(u"Face", 0);

    REQUIRE(epoc::match_font_spec(slanted, u"Face Italic", italic, 20) > epoc::match_font_spec(plain, u"Face", italic, 20));
    REQUIRE(epoc::match_font_spec(plain, u"Face", upright, 20) > epoc::match_font_spec(slanted, u"Face Italic", upright, 20));

    // Monospace is rated above serif, as it changes the layout rather than
    // just the look.
    epoc::font_spec_v1 mono_wanted = make_spec(u"", 20, 0, 0);
    const epoc::open_font_face_attrib mono = make_face(u"Mono", epoc::open_font_face_attrib::mono_width);
    const epoc::open_font_face_attrib serif = make_face(u"Serif", epoc::open_font_face_attrib::serif);

    REQUIRE(epoc::match_font_spec(mono, u"Mono", mono_wanted, 20) > epoc::match_font_spec(serif, u"Serif", mono_wanted, 20));
}

TEST_CASE("font_match_ignores_name_when_none_was_asked_for", "fbs") {
    // An empty typeface name means "whatever fits", and must not hand the 10
    // points to a font whose own name happens to be empty too.
    epoc::font_spec_v1 spec = make_spec(u"", 20, 0);

    const epoc::open_font_face_attrib unnamed = make_face(u"", 0);
    const epoc::open_font_face_attrib named = make_face(u"Nokia Sans S60", 0);

    REQUIRE(epoc::match_font_spec(unnamed, u"", spec, 20) == epoc::match_font_spec(named, u"Nokia Sans S60", spec, 20));
}
