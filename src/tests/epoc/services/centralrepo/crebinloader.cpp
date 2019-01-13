#include <gtest/gtest.h>
#include <epoc/services/centralrepo/cre.h>

#include <common/chunkyseri.h>

#include <fstream>

using namespace eka2l1;

/* Behavior:
 *
 * ROM repo when modified will be stored in a prefer driver (internal if there is one) as binary (2008 forwards)
 * If that drive is not available, it will search for others. Repo ROM modified stored in Private folder/persists
 * 
 * New repo which not based on a ROM one will be stored in the Private folder of a modifiable drive
*/

TEST(centralrepo, generic_cre_loading) {
    central_repo repo;
    std::ifstream fi("centralrepoassets/101f876f.cre", std::ios::binary | std::ios::ate);

    // Read to buffer
    std::vector<char> buf;
    buf.resize(fi.tellg());

    fi.seekg(0, std::ios::beg);
    fi.read(&buf[0], buf.size());

    common::chunkyseri seri(reinterpret_cast<std::uint8_t*>(&buf[0]), common::SERI_MODE_READ);
    do_state_for_cre(seri, repo);

    ASSERT_EQ(repo.uid, 0x101F876F);
    ASSERT_EQ(repo.entries.size(), 19);
    ASSERT_EQ(repo.single_policies.size(), 19);
}