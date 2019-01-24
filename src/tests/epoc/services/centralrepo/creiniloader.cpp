#include <catch2/catch.hpp>
#include <epoc/services/centralrepo/centralrepo.h>

#include <iostream>

using namespace eka2l1;

TEST_CASE("ini_loader_no_default_meta_range", "centralrepo") {
    central_repo repo;

    REQUIRE(parse_new_centrep_ini("centralrepoassets/EFFF0000.ini", repo));
    REQUIRE(repo.entries.size() == 3);

    central_repo_entry *e1 = repo.find_entry(12);
    central_repo_entry *e2 = repo.find_entry(13);
    central_repo_entry *e3 = repo.find_entry(78);

    REQUIRE(e1);
    REQUIRE(e2);
    REQUIRE(e3);

    REQUIRE(e2->data.etype == central_repo_entry_type::real);
    REQUIRE(e2->data.reald == 5.7);

    REQUIRE(e2->data.intd == 15);
    REQUIRE(e3->metadata_val == 12);
}

TEST_CASE("ini_loader_meta_default_and_range", "centralrepo") {
    central_repo repo;

    REQUIRE(parse_new_centrep_ini("centralrepoassets/EFFF0001.ini", repo));
    REQUIRE(repo.entries.size() == 2);

    central_repo_entry *e1 = repo.find_entry(5);
    central_repo_entry *e2 = repo.find_entry(0x42);

    REQUIRE(e1);
    REQUIRE(e2);

    REQUIRE(e1->metadata_val == 10);
    REQUIRE(e2->metadata_val == 12);
}