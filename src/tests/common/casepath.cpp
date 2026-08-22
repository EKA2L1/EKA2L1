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

#include <common/fileutils.h>
#include <common/path.h>

#include <cstdio>
#include <fstream>
#include <string>

using namespace eka2l1;

namespace {
    // A scratch tree under the working directory. The resolver is about real
    // directory entries, so there is nothing to fake here.
    struct scratch_tree {
        std::string root;

        explicit scratch_tree(const std::string &name)
            : root(name) {
            common::delete_folder(root);
            common::create_directories(root);
        }

        ~scratch_tree() {
            common::delete_folder(root);
        }

        void make_dir(const std::string &relative) {
            common::create_directories(eka2l1::add_path(root, relative));
        }

        void make_file(const std::string &relative) {
            const std::string full = eka2l1::add_path(root, relative);
            common::create_directories(eka2l1::file_directory(full));

            std::ofstream out(full, std::ios::binary);
            out.put('x');
        }
    };

    // The resolver only has work to do where the volume distinguishes case. On a
    // volume that does not, the direct lookup already succeeds and every
    // assertion below would hold trivially.
    bool volume_distinguishes_case(const std::string &dir) {
        return !common::is_path_case_insensitive(dir);
    }
}

TEST_CASE("case_insensitive_resolve_finds_entries_stored_in_another_case", "casepath") {
    scratch_tree tree("casepath_resolve");

    if (!volume_distinguishes_case(tree.root)) {
        SUCCEED("volume matches names without regard to case; nothing to resolve");
        return;
    }

    tree.make_file("SIMLIFE/SCRIPT/ISLAND.CFG");

    const std::string resolved = common::resolve_case_insensitive_path(tree.root,
        "simlife\\script\\island.cfg");

    REQUIRE(common::exists(resolved));
    REQUIRE(eka2l1::filename(resolved) == "ISLAND.CFG");
}

TEST_CASE("case_insensitive_resolve_keeps_the_prefix_it_did_resolve", "casepath") {
    scratch_tree tree("casepath_prefix");

    if (!volume_distinguishes_case(tree.root)) {
        SUCCEED("volume matches names without regard to case; nothing to resolve");
        return;
    }

    // A guest creating a save file inside a directory that arrived in upper case.
    // The directory has to resolve even though the file itself is not there yet,
    // otherwise the create lands in a directory that does not exist.
    tree.make_dir("SIMLIFE");

    const std::string resolved = common::resolve_case_insensitive_path(tree.root,
        "simlife\\save.dat");

    REQUIRE_FALSE(common::exists(resolved));
    REQUIRE(common::exists(eka2l1::file_directory(resolved)));
    REQUIRE(eka2l1::filename(resolved) == "save.dat");
}

TEST_CASE("case_insensitive_resolve_leaves_unmatched_components_alone", "casepath") {
    scratch_tree tree("casepath_absent");

    const std::string resolved = common::resolve_case_insensitive_path(tree.root,
        "nowhere\\at\\all.dat");

    // Nothing matched, so the answer is just the path as asked for -- the caller
    // gets the same thing it would have built itself.
    REQUIRE(resolved == common::resolve_case_insensitive_path(tree.root, "nowhere/at/all.dat"));
    REQUIRE(eka2l1::filename(resolved) == "all.dat");
    REQUIRE_FALSE(common::exists(resolved));
}

TEST_CASE("case_insensitive_resolve_passes_through_a_path_already_spelled_right", "casepath") {
    scratch_tree tree("casepath_exact");

    tree.make_file("data/entry.dat");

    const std::string resolved = common::resolve_case_insensitive_path(tree.root,
        "data\\entry.dat");

    REQUIRE(common::exists(resolved));
    REQUIRE(eka2l1::filename(resolved) == "entry.dat");
}

TEST_CASE("path_case_sensitivity_is_answered_per_path_not_per_build", "casepath") {
    scratch_tree tree("casepath_probe");

    // Whatever the answer is for this volume, it has to be stable, and it has to
    // survive being asked about a path that does not exist yet -- callers ask
    // before creating.
    const bool insensitive = common::is_path_case_insensitive(tree.root);

    REQUIRE(common::is_path_case_insensitive(tree.root) == insensitive);
    REQUIRE(common::is_path_case_insensitive(eka2l1::add_path(tree.root, "not/created/yet.dat"))
        == insensitive);

    // And it has to agree with what the filesystem actually does.
    tree.make_file("Probe.dat");
    REQUIRE(common::exists(eka2l1::add_path(tree.root, "probe.dat")) == insensitive);
}
