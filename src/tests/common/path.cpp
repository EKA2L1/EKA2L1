/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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
#include <common/fileutils.h>
#include <common/path.h>
#include <common/platform.h>
#include <cstring>

TEST_CASE("root_name", "path_resolving_test") {
    const std::string example_path_sym = "C:\\symemu\\";
    REQUIRE(eka2l1::root_name(example_path_sym, true) == "C:");
}

TEST_CASE("absolute_checking", "path_resolving_test") {
    const std::string example_path = "Z:\\resource\\";
    const std::string current_dir = "Z:\\sys\\bin\\";

    REQUIRE(eka2l1::is_absolute(example_path, current_dir, true));
}

TEST_CASE("absolute", "path_resolving_test") {
    const std::string example_path = "despacito";
    const std::string current_dir = "Z:\\sys\\bin\\";
    const std::string expected = "Z:\\sys\\bin\\despacito";

    REQUIRE(eka2l1::absolute_path(example_path, current_dir, true) == expected);
}

TEST_CASE("absolute_rooted_path_uses_current_symbian_drive", "path_resolving_test") {
    const std::string example_path = "\\log.txt";
    const std::string current_dir = "E:\\system\\apps\\bowling\\";

    REQUIRE(eka2l1::absolute_path(example_path, current_dir, true) == "E:\\log.txt");
    REQUIRE(eka2l1::absolute_path(std::string("/log.txt"), current_dir, true) == "E:\\log.txt");
    REQUIRE(eka2l1::absolute_path(std::u16string(u"\\log.txt"),
                std::u16string(u"E:\\system\\apps\\bowling\\"), true)
        == u"E:\\log.txt");
}

TEST_CASE("add_path_mess", "path_resolving_test") {
    const std::string example_path = "Z:\\sys/bin\\hi";
    const std::string example_path2 = "ha/ma";
    const std::string expected_result = "Z:\\sys\\bin\\hi\\ha\\ma";

    REQUIRE(eka2l1::add_path(example_path, example_path2, true) == expected_result);
}

TEST_CASE("filename", "path_resolving_test") {
    const std::string example_path = "Z:\\sys\\bin\\euser.dll";
    REQUIRE(strncmp(eka2l1::filename(example_path, true).data(), "euser.dll", 9) == 0);
}

TEST_CASE("filedirectory", "path_resolving_test") {
    const std::string example_path = "Z:\\sys\\bin\\euser.dll";
    REQUIRE(strncmp(eka2l1::file_directory(example_path, true).data(), "Z:\\sys\\bin\\", 11) == 0);
}

TEST_CASE("file_directory_directory_alone", "path_resolving_test") {
    const std::string example_path = "Z:\\sys\\bin\\";
    REQUIRE(strncmp(eka2l1::file_directory(example_path, true).data(), "Z:\\sys\\bin\\", 11) == 0);
}

TEST_CASE("path_iterator", "path_resolving_test") {
    const std::string path = "z:\\private\\10202bef\\";
    const std::string expected_components[3] = { "z:", "private", "10202bef" };

    eka2l1::path_iterator iterator(path);
    std::size_t i = 0;

    for (; iterator; i++, iterator++) {
        if (i >= 3) {
            REQUIRE(*iterator == "");
            break;
        }

        REQUIRE(expected_components[i] == *iterator);
    }

    REQUIRE(i == 3);
}

TEST_CASE("path_iterator_file", "path_resolving_test") {
    const std::string path = "Z:\\sys\\bin\\euser.dll";
    const std::string expected_components[4] = { "Z:", "sys", "bin", "euser.dll" };

    eka2l1::path_iterator iterator(path);
    std::size_t i = 0;

    for (; iterator; i++, iterator++) {
        if (i >= 4) {
            REQUIRE(*iterator == "");
            break;
        }

        REQUIRE(expected_components[i] == *iterator);
    }

    REQUIRE(4 == i);
}

TEST_CASE("extension", "path_resolving_test") {
    std::string test_path = "hiyou.ne.py";
    std::string result_ext = eka2l1::path_extension(test_path);

    REQUIRE(result_ext == ".py");
}

TEST_CASE("replace_extension", "path_resolving_test") {
    std::string test_path = "hiyou.ne.py";
    std::string expected = eka2l1::replace_extension(test_path, ".mom");

    REQUIRE(expected == "hiyou.ne.mom");
}

TEST_CASE("find_case_sensitive_directory_name", "path_resolving_test") {
    REQUIRE(eka2l1::common::find_case_sensitive_file_name(
                "commonassets", "mixedcasedirectory", eka2l1::common::FILE_DIRECTORY)
        == "MixedCaseDirectory");
}

TEST_CASE("copy_folder_lowercases_destination_without_lowercasing_source", "path_resolving_test") {
    const std::string destination = "commonassets-copy-output";
    eka2l1::common::delete_folder(destination);

    REQUIRE(eka2l1::common::copy_folder(
        "commonassets", destination, eka2l1::common::FOLDER_COPY_FLAG_LOWERCASE_NAME));
    REQUIRE(eka2l1::common::exists(
        eka2l1::add_path(destination, "mixedcasedirectory/marker.txt")));

    REQUIRE(eka2l1::common::delete_folder(destination));
}

namespace {
    // The roots are process-wide: put them back so later tests see the default.
    struct runtime_roots_reset {
        ~runtime_roots_reset() {
            eka2l1::set_data_root("");
            eka2l1::set_runtime_resource_root("");
        }
    };
}

TEST_CASE("data_path_is_unchanged_without_a_data_root", "path_resolving_test") {
    runtime_roots_reset reset;
    eka2l1::set_data_root("");

    REQUIRE(eka2l1::data_path("config.yml") == "config.yml");
    REQUIRE(eka2l1::data_path("compat//panicBlackList.json") == "compat//panicBlackList.json");
    REQUIRE(eka2l1::data_path("") == "");
}

TEST_CASE("data_path_resolves_relative_paths_against_the_data_root", "path_resolving_test") {
    runtime_roots_reset reset;
    eka2l1::set_data_root("/srv/eka2l1/instance-a/");

    REQUIRE(eka2l1::data_path("config.yml") == "/srv/eka2l1/instance-a/config.yml");
    REQUIRE(eka2l1::data_path("bindings/default.yml") == "/srv/eka2l1/instance-a/bindings/default.yml");
    REQUIRE(eka2l1::data_path("./cache/") == "/srv/eka2l1/instance-a/cache/");

    // An empty path joined to the working directory named that directory, so an
    // empty storage folder is the data folder itself.
    REQUIRE(eka2l1::data_path("") == "/srv/eka2l1/instance-a/");

    eka2l1::set_data_root("/srv/eka2l1/instance-b");
    REQUIRE(eka2l1::data_path("") == "/srv/eka2l1/instance-b");
}

TEST_CASE("data_path_appends_the_path_as_written", "path_resolving_test") {
    runtime_roots_reset reset;
    eka2l1::set_data_root("/srv/eka2l1/instance-a/");

    // The root stands in for the working directory, so the path must name the
    // same file it named relative to that directory, separators and all.
    REQUIRE(eka2l1::data_path("cache\\") == "/srv/eka2l1/instance-a/cache\\");
    REQUIRE(eka2l1::data_path("patch\\avkonfep_general.dll") == "/srv/eka2l1/instance-a/patch\\avkonfep_general.dll");

    eka2l1::set_data_root("/srv/eka2l1/instance-b");
    REQUIRE(eka2l1::data_path("config.yml") == std::string("/srv/eka2l1/instance-b") + eka2l1::get_separator() + "config.yml");
}

TEST_CASE("data_path_leaves_absolute_paths_alone", "path_resolving_test") {
    runtime_roots_reset reset;
    eka2l1::set_data_root("/srv/eka2l1/instance-a/");

    REQUIRE(eka2l1::data_path("/home/user/banks/my.sf2") == "/home/user/banks/my.sf2");

#if EKA2L1_PLATFORM(WIN32)
    REQUIRE(eka2l1::data_path("\\\\server\\share\\data") == "\\\\server\\share\\data");
    REQUIRE(eka2l1::data_path("\\EKA2L1\\data") == "\\EKA2L1\\data");
    REQUIRE(eka2l1::data_path("D:\\EKA2L1\\data") == "D:\\EKA2L1\\data");
#endif
}

TEST_CASE("data_path_follows_the_host_on_what_is_relative", "path_resolving_test") {
    runtime_roots_reset reset;
    eka2l1::set_data_root("/srv/eka2l1/instance-a/");

#if EKA2L1_PLATFORM(WIN32)
    // On Windows ".\" names the working directory just as "./" does.
    REQUIRE(eka2l1::data_path(".\\cache\\") == "/srv/eka2l1/instance-a/cache\\");
#else
    // Outside Windows these are file names relative to the working directory,
    // so the root has to stand in for it here too.
    REQUIRE(eka2l1::data_path("\\EKA2L1\\data") == "/srv/eka2l1/instance-a/\\EKA2L1\\data");
    REQUIRE(eka2l1::data_path("D:\\EKA2L1\\data") == "/srv/eka2l1/instance-a/D:\\EKA2L1\\data");

    // And ".\" starts a name there, it does not name the working directory.
    REQUIRE(eka2l1::data_path(".\\cache\\") == "/srv/eka2l1/instance-a/.\\cache\\");
#endif
}

TEST_CASE("runtime_resource_path_falls_back_to_the_data_root", "path_resolving_test") {
    runtime_roots_reset reset;
    eka2l1::set_data_root("/srv/eka2l1/instance-a/");

    // Desktop frontends copy the shipped resources into the data folder.
    REQUIRE(eka2l1::runtime_resource_path("resources//brush.vert") == "/srv/eka2l1/instance-a/resources//brush.vert");
}

TEST_CASE("runtime_resource_root_wins_over_the_data_root", "path_resolving_test") {
    runtime_roots_reset reset;
    eka2l1::set_data_root("/srv/eka2l1/instance-a/");
    eka2l1::set_runtime_resource_root("/opt/eka2l1/bundle/");

    REQUIRE(eka2l1::runtime_resource_path(".//patch//") == eka2l1::add_path("/opt/eka2l1/bundle/", "patch//"));
    REQUIRE(eka2l1::runtime_resource_path(".\\patch\\") == eka2l1::add_path("/opt/eka2l1/bundle/", "patch\\"));

    // A path rooted on any host is left alone, whichever host this is.
    REQUIRE(eka2l1::runtime_resource_path("\\EKA2L1\\data") == "\\EKA2L1\\data");
    REQUIRE(eka2l1::runtime_resource_path("D:\\EKA2L1\\data") == "D:\\EKA2L1\\data");
    REQUIRE(eka2l1::data_path("config.yml") == "/srv/eka2l1/instance-a/config.yml");
}
