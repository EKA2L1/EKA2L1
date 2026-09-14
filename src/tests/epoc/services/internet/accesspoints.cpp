/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <catch2/catch.hpp>
#include <services/internet/accesspoints.h>
#include <services/internet/connmonitor.h>
#include <services/centralrepo/cre.h>
#include <common/chunkyseri.h>

using namespace eka2l1;

namespace {
    std::vector<std::uint8_t> serialize(central_repo &repo) {
        common::chunkyseri measure(nullptr, 0, common::SERI_MODE_MEASURE);
        REQUIRE(do_state_for_cre(measure, repo) == 0);
        std::vector<std::uint8_t> result(measure.size());
        common::chunkyseri writer(result.data(), result.size(), common::SERI_MODE_WRITE);
        REQUIRE(do_state_for_cre(writer, repo) == 0);
        return result;
    }

    void add_integer(central_repo &repo, std::uint32_t key, std::uint32_t value) {
        central_repo_entry entry{};
        entry.key = key;
        entry.data.etype = central_repo_entry_type::integer;
        entry.data.intd = value;
        repo.entries.push_back(entry);
    }
}

TEST_CASE("An empty CommsDat exposes a complete host access point", "[internet][host_access_point]") {
    central_repo repo{};
    repo.uid = 0xCCCCCC00;
    provide_host_access_point(repo);
    const auto available = connmonitor_available_iaps(repo);
    REQUIRE(available.count == 1);
    REQUIRE(available.ids[0] == 1);

    // Field IDs and link types follow CommsDatTypeInfoV1_1.h.
    const auto service = repo.find_entry(0x02840100);
    const auto bearer = repo.find_entry(0x02860100);
    const auto network = repo.find_entry(0x02870100);
    REQUIRE(service);
    REQUIRE(bearer);
    REQUIRE(network);
    REQUIRE(repo.find_entry(0x04820000 | (service->data.intd << 8)));
    REQUIRE(repo.find_entry(0x08820000 | (bearer->data.intd << 8)));
    REQUIRE(repo.find_entry(0x01820000 | (network->data.intd << 8)));
    REQUIRE(repo.find_entry(0x0A060100)->data.intd == 1);
    REQUIRE(repo.find_entry(0x0A030100)->data.intd == 1);
    REQUIRE(repo.find_entry(0x03030100));
    REQUIRE(repo.find_entry(0x02FFFF00)->data.etype == central_repo_entry_type::string);
    REQUIRE(repo.find_entry(0x02FF0100)->data.etype == central_repo_entry_type::integer);

    const auto count = repo.entries.size();
    provide_host_access_point(repo);
    REQUIRE(repo.entries.size() == count);
}

TEST_CASE("Host access points preserve existing records and other repositories", "[host_access_point]") {
    central_repo repo{};
    repo.uid = 0xCCCCCC00;
    add_integer(repo, 0x02840100, 8);
    central_repo_entry original_name{};
    original_name.key = 0x02820100;
    original_name.data.etype = central_repo_entry_type::string;
    const std::u16string name = u"Carrier";
    original_name.data.strd.assign(reinterpret_cast<const char *>(name.data()), name.size() * sizeof(char16_t));
    repo.entries.push_back(original_name);
    provide_host_access_point(repo);
    const auto available = connmonitor_available_iaps(repo);
    REQUIRE(available.count == 2);
    REQUIRE(available.ids[0] == 1);
    REQUIRE(available.ids[1] == 2);
    REQUIRE(repo.find_entry(original_name.key)->data.strd == original_name.data.strd);
    REQUIRE(repo.entries.front().data.intd == 8);
    REQUIRE_FALSE(repo.entries.front().transient);
    REQUIRE(repo.find_entry(0x02820200));
    REQUIRE(repo.find_entry(0x0A060100)->data.intd == 2);
    const auto count = repo.entries.size();
    provide_host_access_point(repo);
    REQUIRE(repo.entries.size() == count);

    repo.entries.clear();
    repo.uid = 0x12345678;
    provide_host_access_point(repo);
    REQUIRE(repo.entries.empty());
}

TEST_CASE("Host access point links avoid occupied table records", "[host_access_point]") {
    central_repo repo{};
    repo.uid = 0xCCCCCC00;
    for (const auto table : {0x01800000, 0x02800000, 0x04800000, 0x08800000, 0x03000000, 0x0A000000}) {
        add_integer(repo, table | 0x00010100, 1234);
    }
    provide_host_access_point(repo);
    REQUIRE(repo.find_entry(0x02840200)->data.intd == 2);
    REQUIRE(repo.find_entry(0x02860200)->data.intd == 2);
    REQUIRE(repo.find_entry(0x02870200)->data.intd == 2);
    REQUIRE(repo.find_entry(0x0A030200)->data.intd == 2);
    REQUIRE(repo.find_entry(0x0A060200)->data.intd == 2);
    REQUIRE(repo.find_entry(0x04810100)->data.intd == 1234);
}

TEST_CASE("Host access point records never enter the persisted repository", "[host_access_point]") {
    central_repo repo{};
    repo.ver = 2;
    repo.uid = 0xCCCCCC00;
    add_integer(repo, 0x00123456, 77);
    add_integer(repo, 0x02840100, 8);
    const auto original = serialize(repo);
    provide_host_access_point(repo);
    REQUIRE(serialize(repo) == original);

    add_integer(repo, 0x00123457, 88);
    auto written = serialize(repo);
    central_repo loaded{};
    common::chunkyseri reader(written.data(), written.size(), common::SERI_MODE_READ);
    REQUIRE(do_state_for_cre(reader, loaded) == 0);
    REQUIRE(loaded.entries.size() == 3);
    REQUIRE(loaded.find_entry(0x00123457)->data.intd == 88);
    REQUIRE(connmonitor_available_iaps(loaded).count == 0);
    provide_host_access_point(loaded);
    REQUIRE(connmonitor_available_iaps(loaded).count == 1);
}

TEST_CASE("Full access point or bearer tables prevent partial host records", "[host_access_point]") {
    central_repo repo{};
    repo.uid = 0xCCCCCC00;
    const auto table = GENERATE(0x08810000, 0x02810000);
    for (std::uint32_t id = 1; id < 255; id++) {
        add_integer(repo, table | (id << 8), id);
    }
    provide_host_access_point(repo);
    REQUIRE(repo.entries.size() == 254);
    REQUIRE(connmonitor_available_iaps(repo).count == 0);
}

TEST_CASE("Host access points use an available packet data bearer", "[host_access_point]") {
    central_repo repo{};
    repo.uid = 0xCCCCCC00;
    central_repo_entry bearer{};
    bearer.key = 0x08030200;
    bearer.data.etype = central_repo_entry_type::string;
    const std::u16string nif = u"genericnif";
    bearer.data.strd.assign(reinterpret_cast<const char *>(nif.data()), nif.size() * sizeof(char16_t));
    repo.entries.push_back(bearer);
    provide_host_access_point(repo);
    REQUIRE(repo.find_entry(0x02860100)->data.intd == 2);
    REQUIRE(repo.find_entry(0x0C820100));
    REQUIRE_FALSE(repo.find_entry(0x04820100));
    REQUIRE(repo.find_entry(0x08030200)->data.strd == bearer.data.strd);
    REQUIRE_FALSE(repo.find_entry(0x08030200)->transient);
}
