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
