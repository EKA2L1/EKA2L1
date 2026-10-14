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

#include <common/configure.h>

#ifdef ENABLE_SCRIPTING_LUA

#include <common/buffer.h>
#include <common/fileutils.h>
#include <common/path.h>
#include <common/platform.h>
#include <scripting/manager.h>

#include <string>

using namespace eka2l1;

namespace {
    // Runs a chunk in a fresh Lua state that searches the given scripts folder, and
    // returns what it returned as a string, or the error it raised.
    std::string run_with_scripts_folder(const std::string &folder, const char *chunk, bool &succeeded) {
        lua_State *state = luaL_newstate();
        luaL_openlibs(state);
        manager::add_scripts_folder_searcher(state, folder);

        succeeded = (luaL_dostring(state, chunk) == 0);

        const char *result = lua_tostring(state, -1);
        const std::string result_copy = result ? result : "";

        lua_close(state);
        return result_copy;
    }
}

namespace {
    // A scripts folder under the working directory, removed again when done.
    struct scripts_folder_fixture {
        std::string current_dir;
        std::string folder_name;
        std::string folder;

        scripts_folder_fixture() {
            common::get_current_directory(current_dir);

            // ';' splits package.path into templates and '?' stands for the module name in
            // them (Windows allows no '?' in a name). The rest is "dani" in Cyrillic, as UTF-8.
#if EKA2L1_PLATFORM(WIN32)
            folder_name = "scripts;lua_\xD0\xB4\xD0\xB0\xD0\xBD\xD1\x96";
#else
            folder_name = "scripts;lua?_\xD0\xB4\xD0\xB0\xD0\xBD\xD1\x96";
#endif

            folder = add_path(current_dir, folder_name + get_separator());
            common::delete_folder(folder);
            common::create_directories(add_path(folder, std::string("eka2l1") + get_separator()));
        }

        void write(const std::string &root, const std::string &name, const std::string &source) const {
            common::wo_std_file_stream module(add_path(root, name), true);
            module.write(source.data(), source.size());
        }

        void write_module(const std::string &name, const std::string &source) const {
            write(folder, name, source);
        }
    };
}

TEST_CASE("required_modules_are_found_in_a_scripts_folder_of_any_name", "scripting") {
    scripts_folder_fixture scripts;

    scripts.write_module("eka2l1/answer.lua", "return { answer = 42 }");
    scripts.write_module("eka2l1/broken.lua", "return {");

    // Lua's own file loader skips a first line starting with '#'; so must the searcher.
    scripts.write_module("eka2l1/shebang.lua", "#!/usr/bin/env lua\nreturn { answer = 7 }");

    bool found = false;
    const std::string answer = run_with_scripts_folder(scripts.folder, "return require('eka2l1.answer').answer", found);

    bool shebang_found = false;
    const std::string shebang_answer = run_with_scripts_folder(scripts.folder, "return require('eka2l1.shebang').answer",
        shebang_found);

    bool missing_found = true;
    const std::string missing_error = run_with_scripts_folder(scripts.folder, "return require('eka2l1.missing')", missing_found);

    bool broken_found = true;
    const std::string broken_error = run_with_scripts_folder(scripts.folder, "return require('eka2l1.broken')", broken_found);

    REQUIRE(common::delete_folder(scripts.folder));

    INFO(answer);
    REQUIRE(found);
    REQUIRE(answer == "42");

    INFO(shebang_answer);
    REQUIRE(shebang_found);
    REQUIRE(shebang_answer == "7");

    // A module that is not there is still reported, naming the file looked for in
    // the scripts folder (no path of Lua's own can hold that folder's name).
    INFO(missing_error);
    REQUIRE_FALSE(missing_found);
    REQUIRE(missing_error.find(scripts.folder_name) != std::string::npos);

    // So is one that does not compile.
    REQUIRE_FALSE(broken_found);
    REQUIRE(broken_error.find("error loading module 'eka2l1.broken'") != std::string::npos);
}

TEST_CASE("modules_on_the_lua_path_win_over_the_scripts_folder", "scripting") {
    // The scripts folder used to be the last template of package.path, so a module
    // of the same name anywhere on Lua's own path was found first. That order stays.
    scripts_folder_fixture scripts;

    const std::string path_folder = add_path(scripts.current_dir, std::string("lua_path_first") + get_separator());
    common::delete_folder(path_folder);
    common::create_directories(add_path(path_folder, std::string("eka2l1") + get_separator()));

    scripts.write(path_folder, "eka2l1/shadowed.lua", "return { where = 'lua path' }");
    scripts.write_module("eka2l1/shadowed.lua", "return { where = 'scripts folder' }");
    scripts.write_module("eka2l1/only_here.lua", "return { where = 'scripts folder' }");

    // A long bracket keeps a Windows path's backslashes as they are.
    const std::string prepend_path = "package.path = [==[" + path_folder + "?.lua;]==] .. package.path; ";

    bool shadowed_found = false;
    const std::string shadowed = run_with_scripts_folder(scripts.folder,
        (prepend_path + "return require('eka2l1.shadowed').where").c_str(), shadowed_found);

    bool only_here_found = false;
    const std::string only_here = run_with_scripts_folder(scripts.folder,
        (prepend_path + "return require('eka2l1.only_here').where").c_str(), only_here_found);

    REQUIRE(common::delete_folder(path_folder));
    REQUIRE(common::delete_folder(scripts.folder));

    INFO(shadowed);
    REQUIRE(shadowed_found);
    REQUIRE(shadowed == "lua path");

    INFO(only_here);
    REQUIRE(only_here_found);
    REQUIRE(only_here == "scripts folder");
}

#endif
