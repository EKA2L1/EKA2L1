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
#include <services/centralrepo/repo.h>
#include <services/ui/cap/coestorage.h>
#include <utils/locale.h>

using namespace eka2l1;

TEST_CASE("Automatic Avkon input language resolves before FEP clients start", "[input_language]") {
    central_repo repo{};
    repo.uid = 0x101F876D;
    central_repo_entry entry{};
    entry.key = 5; // KAknFepInputTxtLang: zero selects automatic language.
    entry.data.etype = central_repo_entry_type::integer;
    entry.data.intd = 0;
    repo.entries.push_back(entry);

    SECTION("American UI uses the English PTI keymaps") {
        epoc::initialize_input_language(repo, epoc::lang_american);
        REQUIRE(repo.find_entry(5)->data.intd == epoc::lang_english);
    }
    SECTION("Other UI languages retain their language code") {
        epoc::initialize_input_language(repo, epoc::lang_dutch);
        REQUIRE(repo.find_entry(5)->data.intd == epoc::lang_dutch);
    }
    SECTION("Explicit input language is independent of the UI language") {
        repo.find_entry(5)->data.intd = epoc::lang_french;
        epoc::initialize_input_language(repo, epoc::lang_american);
        REQUIRE(repo.find_entry(5)->data.intd == epoc::lang_french);
    }
    SECTION("Unrelated repositories retain zero values") {
        repo.uid = 0x101F876E;
        epoc::initialize_input_language(repo, epoc::lang_american);
        REQUIRE(repo.find_entry(5)->data.intd == 0);
    }
    SECTION("Missing settings are not invented") {
        repo.entries.clear();
        epoc::initialize_input_language(repo, epoc::lang_american);
        REQUIRE(repo.entries.empty());
    }
    SECTION("Non-integer settings are not reinterpreted") {
        repo.find_entry(5)->data.etype = central_repo_entry_type::string;
        repo.find_entry(5)->data.strd = "unchanged";
        epoc::initialize_input_language(repo, epoc::lang_american);
        REQUIRE(repo.find_entry(5)->data.strd == "unchanged");
        REQUIRE(repo.find_entry(5)->data.etype == central_repo_entry_type::string);
    }
}
