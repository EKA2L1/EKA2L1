#include <gtest/gtest.h>
#include <common/chunkyseri.h>

#include <vector>

using namespace eka2l1;

TEST(chunkyseri, do_read_generic) {
    char *test_data = 
        "\1\0\0\0\5\5\5\5 \0\0\0PEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE";

    common::chunkyseri seri(reinterpret_cast<std::uint8_t*>(test_data), common::SERI_MODE_READ);
    int simple_num1 = 0;
    std::uint32_t simple_num2 = 0;

    seri.absorb(simple_num1);
    seri.absorb(simple_num2);
    
    std::string str;
    seri.absorb(str);

    // Little endian
    // TODO (Big endian)
    ASSERT_EQ(simple_num1, 1);
    ASSERT_EQ(simple_num2, 0x05050505);
    ASSERT_EQ(str, "PEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE");
}

TEST(chunkyseri, do_measure_generic)  {
    common::chunkyseri seri(nullptr, common::SERI_MODE_MESAURE);
    std::uint32_t dummy1;
    std::uint32_t dummy2;

    std::uint16_t dummy3;
    std::string dummy4 = "LOOL";
    std::u16string dummy5 = u"LOOL";

    seri.absorb(dummy1);
    seri.absorb(dummy2);
    seri.absorb(dummy3);
    seri.absorb(dummy4);
    seri.absorb(dummy5);

    // Calculate size
    std::size_t expected = sizeof(dummy1) + sizeof(dummy2) + sizeof(dummy3) + 4 + dummy4.length()
        + 4 + dummy5.length() * 2;

    ASSERT_EQ(seri.size(), expected);
}

TEST(chunkyseri, do_write_generic) {
    std::vector<std::uint8_t> buf;
    buf.resize(32);

    std::uint64_t d1 = 0xDFFFFFFFFF;
    std::uint32_t d2 = 15;

    std::string d3 = "HELLO!";

    common::chunkyseri seri(&buf[0], common::SERI_MODE_WRITE);
    seri.absorb(d1);
    seri.absorb(d2);
    seri.absorb(d3);

    std::uint8_t *bufptr = &buf[0];
    ASSERT_EQ(d1, *reinterpret_cast<decltype(d1)*>(bufptr));
    bufptr += sizeof(d1);
    
    ASSERT_EQ(d2, *reinterpret_cast<decltype(d2)*>(bufptr));
    bufptr += sizeof(d2);

    ASSERT_EQ(d3.length(), *reinterpret_cast<std::uint32_t*>(bufptr));
    bufptr += 4;

    ASSERT_STREQ(d3.c_str(), reinterpret_cast<char*>(bufptr));
}

TEST(chunkyseri, do_read_with_section) {
    char *buf = 
        "TestSection\1\0\5\0\7\0\7\0\0\0HIPEOPL";

    common::chunkyseri seri(reinterpret_cast<std::uint8_t*>(buf), common::SERI_MODE_READ);
    ASSERT_TRUE(seri.section("TestSection", 1));

    std::uint16_t t1 = 0;
    std::uint16_t t2 = 0;

    std::string t3 {};

    seri.absorb(t1);
    seri.absorb(t2);
    seri.absorb(t3);

    ASSERT_EQ(t1, 5);
    ASSERT_EQ(t2, 7);
    ASSERT_EQ(t3, "HIPEOPL");
}