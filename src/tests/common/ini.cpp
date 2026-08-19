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
#include <common/ini.h>

using namespace eka2l1;

TEST_CASE("two_prop_in_a_section", "ini_test") {
    common::ini_file ini;
    ini.load("commonassets/test.ini");

    auto sec1_ptr = ini.find("section1");

    REQUIRE(sec1_ptr);

    common::ini_section *sec = sec1_ptr->get_as<common::ini_section>();

    auto prop1_ptr = sec->find("prop1");
    auto prop2_ptr = sec->find("prop2");

    REQUIRE(prop1_ptr);
    REQUIRE(prop2_ptr);

    REQUIRE(prop1_ptr->get_node_type() == common::ini_node_type::INI_NODE_PAIR);
    REQUIRE(prop2_ptr->get_node_type() == common::ini_node_type::INI_NODE_PAIR);

    REQUIRE(prop1_ptr->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value() == "2");
    REQUIRE(prop2_ptr->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value() == "4");
}

TEST_CASE("quoted_value_keeps_separators", "ini_test") {
    common::ini_file ini;
    ini.load("commonassets/test3.ini");

    auto sec_ptr = ini.find("quoted");
    REQUIRE(sec_ptr);

    common::ini_section *sec = sec_ptr->get_as<common::ini_section>();

    // A quoted value ends at its closing quote. Commas, tabs and '=' inside it are
    // ordinary characters: central repository string entries pack whole
    // comma-separated setting lists into one quoted value.
    auto prop_ptr = sec->find("0x10001");
    REQUIRE(prop_ptr);

    common::ini_pair *prop = prop_ptr->get_as<common::ini_pair>();
    REQUIRE(prop->get<common::ini_value>(0)->get_value() == "string");
    REQUIRE(prop->get<common::ini_value>(1)->get_value()
        == "QualitySetLevel=98,VideoCodecMimeType=video/mp4v-es; profile-level-id=2,VideoWidth=176");

    // The closing quote is consumed, so what trails it tokenizes normally.
    REQUIRE(prop->get<common::ini_value>(2)->get_value() == "0");

    prop_ptr = sec->find("0x10002");
    REQUIRE(prop_ptr);

    prop = prop_ptr->get_as<common::ini_pair>();
    REQUIRE(prop->get<common::ini_value>(1)->get_value() == "plain");
    REQUIRE(prop->get<common::ini_value>(2)->get_value() == "5");
}

TEST_CASE("recursive_prop_with_comma", "ini_test") {
    common::ini_file ini;
    ini.load("commonassets/test2.ini");

    auto sec1_ptr = ini.find("test");

    REQUIRE(sec1_ptr);

    common::ini_section *sec = sec1_ptr->get_as<common::ini_section>();

    auto prop1_ptr = sec->find("aprop");
    REQUIRE(prop1_ptr);

    REQUIRE(prop1_ptr->get_node_type() == common::ini_node_type::INI_NODE_PAIR);
    REQUIRE(prop1_ptr->get_as<common::ini_pair>()->get_value_count() == 3);

    REQUIRE(prop1_ptr->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value() == "5");
    REQUIRE(prop1_ptr->get_as<common::ini_pair>()->get<common::ini_value>(1)->get_value() == "12");
    REQUIRE(prop1_ptr->get_as<common::ini_pair>()->get<common::ini_value>(2)->get_value() == "22");

    sec1_ptr = ini.find("test2");
    REQUIRE(sec1_ptr);

    prop1_ptr = sec1_ptr->get_as<common::ini_section>()->find("MOM");
    REQUIRE(prop1_ptr);

    REQUIRE(prop1_ptr->get_node_type() == common::ini_node_type::INI_NODE_PAIR);
    REQUIRE(prop1_ptr->get_as<common::ini_pair>()->get_value_count() == 4);

    common::ini_pair *p = prop1_ptr->get_as<common::ini_pair>()->get<common::ini_pair>(1);

    REQUIRE(p->get<common::ini_value>(0)->get_value() == "5");
    REQUIRE(p->get<common::ini_value>(1)->get_value() == "7");
    REQUIRE(p->get<common::ini_value>(2)->get_value() == "9");
}
