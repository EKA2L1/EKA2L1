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
#include <gdbstub/gdbstub.h>

#include <string>
#include <vector>

using namespace eka2l1;

TEST_CASE("library_list_gives_code_then_data_segment", "gdbstub") {
    const std::vector<gdb_library> libraries = {
        { "Z:\\sys\\bin\\euser.dll", 0x80123000, 0 },
        { "C:\\sys\\bin\\hello.dll", 0x70004000, 0x00400000 }
    };

    REQUIRE(make_gdb_library_list(libraries) == "<?xml version=\"1.0\"?>"
                                                "<library-list version=\"1.0\">"
                                                "<library name=\"Z:\\sys\\bin\\euser.dll\"><segment address=\"0x80123000\"/></library>"
                                                "<library name=\"C:\\sys\\bin\\hello.dll\"><segment address=\"0x70004000\"/><segment address=\"0x400000\"/></library>"
                                                "</library-list>");
}

TEST_CASE("library_list_leaves_out_the_main_executable", "gdbstub") {
    // GDB loads the main executable with "file". Listed as a library as well, it would get a
    // second copy of its symbols at the run address, and "break E32Main" two locations.
    const std::vector<gdb_library> libraries = {
        { "Z:\\sys\\bin\\euser.dll", 0x80123000, 0, false },
        { "C:\\sys\\bin\\app.exe", 0x70000000, 0x00400000, true },
        { "C:\\sys\\bin\\helper.dll", 0x70004000, 0x00600000, false }
    };

    REQUIRE(make_gdb_library_list(libraries) == "<?xml version=\"1.0\"?>"
                                                "<library-list version=\"1.0\">"
                                                "<library name=\"Z:\\sys\\bin\\euser.dll\"><segment address=\"0x80123000\"/></library>"
                                                "<library name=\"C:\\sys\\bin\\helper.dll\"><segment address=\"0x70004000\"/><segment address=\"0x600000\"/></library>"
                                                "</library-list>");
}

TEST_CASE("library_list_escapes_names", "gdbstub") {
    const std::vector<gdb_library> libraries = { { "C:\\a&b<c>\"d'.dll", 0x1000, 0 } };

    REQUIRE(make_gdb_library_list(libraries) == "<?xml version=\"1.0\"?>"
                                                "<library-list version=\"1.0\">"
                                                "<library name=\"C:\\a&amp;b&lt;c&gt;&quot;d&apos;.dll\"><segment address=\"0x1000\"/></library>"
                                                "</library-list>");
}

TEST_CASE("library_list_empty", "gdbstub") {
    REQUIRE(make_gdb_library_list({}) == "<?xml version=\"1.0\"?><library-list version=\"1.0\"></library-list>");
}

TEST_CASE("xfer_reply_marks_more_and_last", "gdbstub") {
    const std::string document = "abcdef";

    REQUIRE(make_gdb_xfer_reply(document, 0, 4, 100) == "mabcd");
    REQUIRE(make_gdb_xfer_reply(document, 4, 4, 100) == "lef");
    REQUIRE(make_gdb_xfer_reply(document, 0, 6, 100) == "labcdef");
    REQUIRE(make_gdb_xfer_reply(document, 6, 4, 100) == "l");
    REQUIRE(make_gdb_xfer_reply(document, 10, 4, 100) == "l");
}

TEST_CASE("xfer_reply_escapes_binary_data", "gdbstub") {
    // '#', '$', '}' and '*' go out as '}' followed by the byte XOR 0x20.
    REQUIRE(make_gdb_xfer_reply("a#b$c}d*e", 0, 100, 100) == "la}\x03"
                                                            "b}\x04"
                                                            "c}]d}\x0a"
                                                            "e");
}

TEST_CASE("xfer_reply_stops_before_exceeding_the_size_limit", "gdbstub") {
    // The marker takes one byte and every escaped byte two, so only two of the four fit.
    REQUIRE(make_gdb_xfer_reply("####", 0, 4, 5) == "m}\x03}\x03");
    REQUIRE(make_gdb_xfer_reply("####", 2, 4, 5) == "l}\x03}\x03");
}
