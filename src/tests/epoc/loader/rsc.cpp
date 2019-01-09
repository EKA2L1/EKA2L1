#include <epoc/loader/rsc.h>
#include <gtest/gtest.h>

#include <epoc/vfs.h>

using namespace eka2l1;

// Syncing result with Symbian's CResourceFile class
TEST(rsc_file, no_compress_but_may_contain_unicode) {
    const char *rsc_name = "sample_0xed3e09d5.rsc";
    symfile f = eka2l1::physical_file_proxy(rsc_name, READ_MODE | BIN_MODE);

    loader::rsc_file test_rsc(f);
    ASSERT_EQ(test_rsc.get_total_resources(), 11);
}