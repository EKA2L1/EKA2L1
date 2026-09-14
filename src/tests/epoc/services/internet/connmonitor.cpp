#include <catch2/catch.hpp>

#include <services/centralrepo/repo.h>
#include <services/internet/connmonitor.h>

using namespace eka2l1;

namespace {
    central_repo_entry name_entry(std::uint32_t key) {
        central_repo_entry entry{};
        entry.key = key;
        entry.data.etype = central_repo_entry_type::string;
        entry.data.strd = std::string("I\0A\0P\0", 6);
        return entry;
    }
}

TEST_CASE("Connection Monitor enumerates CommsDat IAP record IDs", "[internet][connmonitor]") {
    central_repo repo{};
    // commsdattypeinfov1_1.h and commsdat.h: IAP RecordName and attribute/record masks.
    repo.entries = {name_entry(0x02820720), name_entry(0x02820200), name_entry(0x02820000),
        name_entry(0x0282FF00), name_entry(0x03020500), name_entry(0x02820220)};
    const auto info = connmonitor_available_iaps(repo);
    REQUIRE(info.count == 2);
    REQUIRE(info.ids[0] == 2);
    REQUIRE(info.ids[1] == 7);
    REQUIRE(info.ids[2] == 0);
    REQUIRE(sizeof(info) == 104); // rconnmon.h: TUint count followed by 25 TConnMonIap values.
}

TEST_CASE("Connection Monitor bounds the native IAP package", "[internet][connmonitor]") {
    central_repo repo{};
    REQUIRE(connmonitor_available_iaps(repo).count == 0);
    for (std::uint32_t id = 1; id <= 30; id++) {
        repo.entries.push_back(name_entry(0x02820000 | (id << 8)));
    }
    const auto info = connmonitor_available_iaps(repo);
    REQUIRE(info.count == 25);
    REQUIRE(info.ids.front() == 1);
    REQUIRE(info.ids.back() == 25);
}
