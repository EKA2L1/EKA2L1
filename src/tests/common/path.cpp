#include <common/path.h>
#include <gtest/gtest.h>

TEST(path_resolving_test, root_name) {
    const std::string example_path_sym = "C:\\symemu\\";
    ASSERT_EQ(eka2l1::root_name(example_path_sym, true), "C:");
}

TEST(path_resolving_test, absolute_checking) {
    const std::string example_path = "Z:\\resource\\";
    const std::string current_dir = "Z:\\sys\\bin\\";

    ASSERT_TRUE(eka2l1::is_absolute(example_path, current_dir, true));
}

TEST(path_resolving_test, absolute) {
    const std::string example_path = "despacito";
    const std::string current_dir = "Z:\\sys\\bin\\";
    const std::string expected = "Z:\\sys\\bin\\despacito";

    ASSERT_EQ(eka2l1::absolute_path(example_path, current_dir, true), expected);
}

TEST(path_resolving_test, add_path_mess) {
    const std::string example_path = "Z:\\sys/bin\\hi";
    const std::string example_path2 = "ha/ma";
    const std::string expected_result = "Z:\\sys\\bin\\hi\\ha\\ma";
    
    ASSERT_EQ(eka2l1::add_path(example_path, example_path2, true), expected_result);
}

TEST(path_resolving_test, filename) {
    const std::string example_path = "Z:\\sys\\bin\\euser.dll";
    ASSERT_EQ(strncmp(eka2l1::filename(example_path, true).data(), "euser.dll", 9), 0);
}