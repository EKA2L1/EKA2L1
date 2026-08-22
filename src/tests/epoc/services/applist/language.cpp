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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <catch2/catch.hpp>

#include <common/types.h>

// AppList::AppLanguage writes this enum straight into the guest's TLanguage, so
// the two numberings have to agree. Reference values are Symbian's own TLanguage,
// which carries its indices as comments in epoc32/include/aiftool.rh
// (ELangTest // 00, ELangEnglish // 01, ...).
//
// Numbering only started to matter when the reply stopped being a constant, so
// these anchors sample the whole run rather than just the low end. Note that
// index 95 and 99 do not line up with TLanguage (ELangUzbek and ELangOther);
// that predates this and is left alone here.

namespace {
    constexpr int lang_index(const language lang) {
        return static_cast<int>(lang);
    }
}

TEST_CASE("language_enum_agrees_with_symbian_tlanguage_numbering", "applist_language") {
    REQUIRE(lang_index(language::test) == 0);   // ELangTest
    REQUIRE(lang_index(language::en) == 1);     // ELangEnglish
    REQUIRE(lang_index(language::fr) == 2);     // ELangFrench
    REQUIRE(lang_index(language::de) == 3);     // ELangGerman
    REQUIRE(lang_index(language::fi) == 9);     // ELangFinnish

    // The block Symbian inserts before Portuguese; getting it wrong shifts
    // everything above it by three.
    REQUIRE(lang_index(language::am) == 10);    // ELangAmerican
    REQUIRE(lang_index(language::sf) == 11);    // ELangSwissFrench
    REQUIRE(lang_index(language::sg) == 12);    // ELangSwissGerman
    REQUIRE(lang_index(language::po) == 13);    // ELangPortuguese

    REQUIRE(lang_index(language::ru) == 16);    // ELangRussian
    REQUIRE(lang_index(language::hr) == 45);    // ELangCroatian
    REQUIRE(lang_index(language::et) == 49);    // ELangEstonian
    REQUIRE(lang_index(language::uk) == 93);    // ELangUkrainian
    REQUIRE(lang_index(language::ur) == 94);    // ELangUrdu
    REQUIRE(lang_index(language::vi) == 96);    // ELangVietnamese
    REQUIRE(lang_index(language::cy) == 97);    // ELangWelsh
    REQUIRE(lang_index(language::zu) == 98);    // ELangZulu
}

TEST_CASE("app_language_fallback_only_catches_the_non_languages", "applist_language") {
    // The values applist_server::app_language replaces with English. Everything
    // between them is a real TLanguage and must pass straight through.
    REQUIRE(language::test < language::en);
    REQUIRE_FALSE(language::fr < language::en);
    REQUIRE_FALSE(language::zu < language::en);
    REQUIRE(lang_index(language::any) > lang_index(language::zu));
}
