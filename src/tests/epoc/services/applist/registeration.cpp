/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <loader/rsc.h>
#include <services/applist/applist.h>
#include <vfs/vfs.h>

#include <common/algorithm.h>
#include <common/buffer.h>

#include <catch2/catch.hpp>

using namespace eka2l1;

static bool read_resource_from_file(const char *name, const int id, std::vector<std::uint8_t> &dat,
    bool confirm_sig = false) {
    symfile f = eka2l1::physical_file_proxy(name, READ_MODE | BIN_MODE);

    if (!f) {
        return false;
    }

    eka2l1::ro_file_stream std_rsc_raw(f.get());
    if (!std_rsc_raw.valid()) {
        return false;
    }

    loader::rsc_file std_rsc(reinterpret_cast<common::ro_stream *>(&std_rsc_raw));

    if (confirm_sig) {
        std_rsc.confirm_signature();
    }

    dat = std_rsc.read(id);

    return true;
}

TEST_CASE("mandatory_check_non_localise", "applist_registeration") {
    std::vector<std::uint8_t> dat;
    REQUIRE(read_resource_from_file("applistassets//sample_reg.rsc", 1, dat));

    common::ro_buf_stream app_info_resource_stream(&dat[0], dat.size());
    apa_app_registry reg;

    bool result = read_registeration_info(reinterpret_cast<common::ro_stream *>(&app_info_resource_stream),
        reg, drive_c, true);

    REQUIRE(result);
    REQUIRE(common::compare_ignore_case(reg.mandatory_info.app_path.to_std_string(nullptr),
                u"C:\\System\\Programs\\ITried_0xed3e09d5.exe")
        == 0);
    REQUIRE(reg.caps.flags == 0);
    REQUIRE(reg.caps.ability == apa_capability::embeddability::not_embeddable);
    REQUIRE(reg.default_screen_number == 0);
}

TEST_CASE("mandatory_check_localise", "applist_registeration") {
    std::vector<std::uint8_t> dat;
    REQUIRE(read_resource_from_file("applistassets//localised_sample_reg.rsc", 0x2EEDE00B, dat, true));

    common::ro_buf_stream app_info_resource_stream(&dat[0], dat.size());
    apa_app_registry reg;

    bool result = read_localised_registration_info(
        reinterpret_cast<common::ro_stream *>(&app_info_resource_stream), reg, drive_c);

    REQUIRE(result);
    REQUIRE(reg.mandatory_info.short_caption.to_std_string(nullptr) == u"ITried");
    REQUIRE(reg.mandatory_info.long_caption.to_std_string(nullptr) == u"ITried");
}

static std::vector<std::uint8_t> registration_with_mime_types() {
    std::vector<std::uint8_t> data(8, 0);
    const auto append_string16 = [&data](const std::u16string &value) {
        data.push_back(static_cast<std::uint8_t>(value.size()));
        if (!value.empty()) {
            if (data.size() & 1) {
                data.push_back(0);
            }
            for (const auto ch : value) {
                data.push_back(ch & 0xFF);
                data.push_back(ch >> 8);
            }
        }
    };
    append_string16(u"MimeTest");
    data.insert(data.end(), 4, 0); // Attributes.
    append_string16(u"");
    data.insert(data.end(), 8, 0); // Localisable resource ID and capabilities.
    append_string16(u"");
    data.push_back(0); // Default screen.
    data.insert(data.end(), { 2, 0 });
    for (const auto &type : { std::string("application/x-shockwave-flash"), std::string("video/x-flv") }) {
        data.insert(data.end(), { 0x78, 0x56, 0x34, 0x12 });
        data.push_back(static_cast<std::uint8_t>(type.size()));
        data.insert(data.end(), type.begin(), type.end());
    }
    return data;
}

TEST_CASE("registration_reads_mime_types_and_full_width_priorities", "applist_registeration") {
    auto data = registration_with_mime_types();
    common::ro_buf_stream stream(data.data(), data.size());
    apa_app_registry reg;

    REQUIRE(read_registeration_info(&stream, reg, drive_z, false));
    REQUIRE(reg.data_types.size() == 2);
    REQUIRE(reg.data_types[0].type_ == "application/x-shockwave-flash");
    REQUIRE(reg.data_types[1].type_ == "video/x-flv");
    REQUIRE(reg.data_types[0].priority_ == 0x12345678);
    REQUIRE(reg.data_types[1].priority_ == 0x12345678);
    reg.mandatory_info.uid = 0x101FD693;
    REQUIRE(find_data_type_handler({ reg }, "application/x-shockwave-flash") == 0x101FD693);
}

TEST_CASE("truncated_mime_list_preserves_mandatory_registration", "applist_registeration") {
    auto data = registration_with_mime_types();
    data.pop_back();
    common::ro_buf_stream stream(data.data(), data.size());
    apa_app_registry reg;

    REQUIRE(read_registeration_info(&stream, reg, drive_z, false));
    REQUIRE(reg.mandatory_info.app_path.to_std_string(nullptr) == u"Z:\\MimeTest.exe");
    REQUIRE(reg.data_types.empty());
}
