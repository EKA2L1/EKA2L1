#include <common/ini.h>
#include <gtest/gtest.h>

using namespace eka2l1;

TEST(ini_test, two_prop_in_a_section) {
    common::ini_file ini;
    ini.load("commonassets/test.ini");

    auto sec1_ptr = ini.find("section1");

    ASSERT_TRUE(sec1_ptr);

    common::ini_section *sec = sec1_ptr->get_as<common::ini_section>();

    auto prop1_ptr = sec->find("prop1");
    auto prop2_ptr = sec->find("prop2");

    ASSERT_TRUE(prop1_ptr);
    ASSERT_TRUE(prop2_ptr);

    ASSERT_EQ(prop1_ptr->get_node_type(), common::ini_node_type::INI_NODE_KEY);
    ASSERT_EQ(prop2_ptr->get_node_type(), common::ini_node_type::INI_NODE_KEY);

    ASSERT_EQ(prop1_ptr->get_as<common::ini_prop>()->get_value(), "2");
    ASSERT_EQ(prop2_ptr->get_as<common::ini_prop>()->get_value(), "4");
}