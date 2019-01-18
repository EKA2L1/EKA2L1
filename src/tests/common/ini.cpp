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

    ASSERT_EQ(prop1_ptr->get_node_type(), common::ini_node_type::INI_NODE_PAIR);
    ASSERT_EQ(prop2_ptr->get_node_type(), common::ini_node_type::INI_NODE_PAIR);

    ASSERT_EQ(prop1_ptr->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value(), "2");
    ASSERT_EQ(prop2_ptr->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value(), "4");
}

TEST(ini_test, recursive_prop_with_comman) {
    common::ini_file ini;
    ini.load("commonassets/test2.ini");

    auto sec1_ptr = ini.find("test");

    ASSERT_TRUE(sec1_ptr);

    common::ini_section *sec = sec1_ptr->get_as<common::ini_section>();

    auto prop1_ptr = sec->find("aprop");
    ASSERT_TRUE(prop1_ptr);

    ASSERT_EQ(prop1_ptr->get_node_type(), common::ini_node_type::INI_NODE_PAIR);
    ASSERT_EQ(prop1_ptr->get_as<common::ini_pair>()->get_value_count(), 3);

    ASSERT_EQ(prop1_ptr->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value(), "5");
    ASSERT_EQ(prop1_ptr->get_as<common::ini_pair>()->get<common::ini_value>(1)->get_value(), "12");
    ASSERT_EQ(prop1_ptr->get_as<common::ini_pair>()->get<common::ini_value>(2)->get_value(), "22");

    sec1_ptr = ini.find("test2");
    ASSERT_TRUE(sec1_ptr);

    prop1_ptr = sec1_ptr->get_as<common::ini_section>()->find("MOM");
    ASSERT_TRUE(prop1_ptr);

    ASSERT_EQ(prop1_ptr->get_node_type(), common::ini_node_type::INI_NODE_PAIR);
    ASSERT_EQ(prop1_ptr->get_as<common::ini_pair>()->get_value_count(), 4);

    common::ini_pair *p = prop1_ptr->get_as<common::ini_pair>()->get<common::ini_pair>(1);

    ASSERT_EQ(p->get<common::ini_value>(0)->get_value(), "5");
    ASSERT_EQ(p->get<common::ini_value>(1)->get_value(), "7");
    ASSERT_EQ(p->get<common::ini_value>(2)->get_value(), "9");
}