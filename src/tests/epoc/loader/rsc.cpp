#include <epoc/loader/rsc.h>
#include <catch2/catch.hpp>

#include <epoc/vfs.h>

#include <fstream>
#include <sstream>

using namespace eka2l1;

// Syncing result with Symbian's CResourceFile class
TEST_CASE("no_compress_but_may_contain_unicode", "rsc_file") {
    const char *rsc_name = "sample_0xed3e09d5.rsc";
    symfile f = eka2l1::physical_file_proxy(rsc_name, READ_MODE | BIN_MODE);

    loader::rsc_file test_rsc(f);

    const std::uint16_t total_res = 11;
    REQUIRE(test_rsc.get_total_resources() == total_res);

    // Iterate through each resources
    for (int i = 1; i <= total_res; i++) {
        std::stringstream ss;
        ss << "loaderassets//SAMPLE_RESOURCE_DATA_IDX_";
        ss << i;
        ss << ".bin";

        std::ifstream fi(ss.str(), std::ios::ate | std::ios::binary);
        const std::size_t res_size = fi.tellg();

        std::vector<std::uint8_t> res_from_eka2l1 = test_rsc.read(i);
        fi.seekg(0, std::ios::beg);

        std::vector<std::uint8_t> expected_res;
        expected_res.resize(res_size);

        fi.read(reinterpret_cast<char*>(&expected_res[0]), res_size); 

        REQUIRE(res_from_eka2l1.size() == res_size);
        REQUIRE(expected_res == res_from_eka2l1);
    }
}