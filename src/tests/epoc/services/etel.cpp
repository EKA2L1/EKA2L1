// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch.hpp>
#include <services/etel/phone.h>

using namespace eka2l1;

TEST_CASE("ETel V1 network packages match the EKA1 GSM client ABI", "[etel]") {
    // N70 Gsmbas passes 0xF8 and 0x10 byte packages, then reads these fields.
    epoc::etel_phone_network_info_v1 network{};
    const auto *base = reinterpret_cast<const std::uint8_t *>(&network);
    REQUIRE(sizeof(network) == 0xF8);
    REQUIRE(reinterpret_cast<const std::uint8_t *>(&network.mode_) - base == 4);
    REQUIRE(reinterpret_cast<const std::uint8_t *>(&network.status_) - base == 8);
    REQUIRE(reinterpret_cast<const std::uint8_t *>(&network.country_code_) - base == 0x10);
    REQUIRE(reinterpret_cast<const std::uint8_t *>(&network.short_name_) - base == 0xAC);
    REQUIRE(reinterpret_cast<const std::uint8_t *>(&network.long_name_) - base == 0xC8);

    epoc::etel_phone_location_area location{};
    base = reinterpret_cast<const std::uint8_t *>(&location);
    REQUIRE(sizeof(location) == 0x10);
    REQUIRE(sizeof(location.area_known_) == 4);
    REQUIRE(reinterpret_cast<const std::uint8_t *>(&location.location_area_code_) - base == 8);
    REQUIRE(reinterpret_cast<const std::uint8_t *>(&location.cell_id_) - base == 12);

    epoc::etel_phone_network_info extended{};
    base = reinterpret_cast<const std::uint8_t *>(&extended);
    REQUIRE(reinterpret_cast<const std::uint8_t *>(&extended.access_) - base == 0xF8);
}

TEST_CASE("Phone network queries before Initialise start with an unknown network", "[etel]") {
    epoc::etel_phone_info info{};
    etel_phone phone(info);
    REQUIRE(phone.network_info_.mode_ == epoc::etel_mobile_phone_network_mode_unknown);
    REQUIRE(phone.network_info_.status_ == epoc::etel_mobile_phone_network_status_unknown);
    REQUIRE(phone.network_info_.band_info_ == epoc::etel_mobile_phone_band_unknown);
    REQUIRE(phone.network_info_.country_code_.get_length() == 0);
    REQUIRE(phone.network_info_.network_id_.get_length() == 0);
    REQUIRE(phone.network_info_.short_name_.get_length() == 0);
    REQUIRE(phone.network_info_.long_name_.get_length() == 0);
}
