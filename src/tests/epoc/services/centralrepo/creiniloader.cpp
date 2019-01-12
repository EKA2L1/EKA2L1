#include <gtest/gtest.h>
#include <epoc/services/centralrepo/centralrepo.h>

#include <iostream>

using namespace eka2l1;

TEST(centralrepo, ini_loader_no_default_meta_range) {
    central_repo repo;

    ASSERT_TRUE(parse_new_centrep_ini("centralrepoassets/EFFF0000.ini", repo));
    ASSERT_EQ(repo.entries.size(), 3);

    central_repo_entry *e1 = repo.find_entry(12);
    central_repo_entry *e2 = repo.find_entry(13);
    central_repo_entry *e3 = repo.find_entry(78);

    ASSERT_TRUE(e1);
    ASSERT_TRUE(e2);
    ASSERT_TRUE(e3);

    ASSERT_EQ(e2->data.etype, central_repo_entry_type::real);
    ASSERT_EQ(e2->data.reald, 5.7);

    ASSERT_EQ(e2->data.intd, 15);
    ASSERT_EQ(e3->metadata_val, 12);
}