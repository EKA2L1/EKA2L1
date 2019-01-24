#include <catch2/catch.hpp>
#include <common/chunkyseri.h>

#include <vector>

using namespace eka2l1;

TEST_CASE("do_read_generic", "chunkyseri") {
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
    REQUIRE(simple_num1 == 1);
    REQUIRE(simple_num2 == 0x05050505);
    REQUIRE(str == "PEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE");
}

TEST_CASE("do_measure_generic", "chunkyseri")  {
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

    REQUIRE(seri.size() == expected);
}

TEST_CASE("do_write_generic", "chunkyseri") {
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
    REQUIRE(d1 == *reinterpret_cast<decltype(d1)*>(bufptr));
    bufptr += sizeof(d1);
    
    REQUIRE(d2 == *reinterpret_cast<decltype(d2)*>(bufptr));
    bufptr += sizeof(d2);

    REQUIRE(d3.length() == *reinterpret_cast<std::uint32_t*>(bufptr));
    bufptr += 4;

    REQUIRE(d3 == reinterpret_cast<char*>(bufptr));
}

TEST_CASE("do_read_with_section", "chunkyseri") {
    char *buf = 
        "TestSection\1\0\5\0\7\0\7\0\0\0HIPEOPL";

    common::chunkyseri seri(reinterpret_cast<std::uint8_t*>(buf), common::SERI_MODE_READ);
    REQUIRE(seri.section("TestSection", 1));

    std::uint16_t t1 = 0;
    std::uint16_t t2 = 0;

    std::string t3 {};

    seri.absorb(t1);
    seri.absorb(t2);
    seri.absorb(t3);

    REQUIRE(t1 == 5);
    REQUIRE(t2 == 7);
    REQUIRE(t3 == "HIPEOPL");
}