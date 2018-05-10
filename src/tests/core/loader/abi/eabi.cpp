#include <gtest/gtest.h>
#include <abi/eabi.h>

using namespace eka2l1;

TEST(demangleDestructor, demanglingSomeDestructors) {
    const auto target1 = "_ZN9wikipedia7article8wikilinkC1ERKSs";
    const auto res1 = "wikipedia::article::wikilink(const std::string&)";
   
    ASSERT_EQ(eabi::demangle(target1), res1);
}
