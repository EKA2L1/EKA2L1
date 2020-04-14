/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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
#include <epoc/loader/rsc.h>

#include <common/buffer.h>
#include <epoc/vfs.h>

#include <fstream>
#include <sstream>
#include <vector>

using namespace eka2l1;

// Syncing result with Symbian's CResourceFile class
TEST_CASE("no_compress_but_may_contain_unicode", "rsc_file") {
    const char *rsc_name = "loaderassets//sample_0xed3e09d5.rsc";
    symfile f = eka2l1::physical_file_proxy(rsc_name, READ_MODE | BIN_MODE);

    REQUIRE(f);

    std::vector<std::uint8_t> buf;
    buf.resize(f->size());
    f->read_file(reinterpret_cast<std::uint8_t *>(&buf[0]), 1, static_cast<std::uint32_t>(buf.size()));

    f->close();

    common::ro_buf_stream stream(&buf[0], buf.size());
    loader::rsc_file test_rsc(reinterpret_cast<common::ro_stream *>(&stream));

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

        fi.read(reinterpret_cast<char *>(&expected_res[0]), res_size);

        REQUIRE(res_from_eka2l1.size() == res_size);
        REQUIRE(expected_res == res_from_eka2l1);
    }
}

TEST_CASE("no_compress_but_may_contain_unicode_2", "rsc_file") {
    const char *rsc_name = "loaderassets//javadrmmanager.rsc";
    symfile f = eka2l1::physical_file_proxy(rsc_name, READ_MODE | BIN_MODE);

    REQUIRE(f);

    std::vector<std::uint8_t> buf;
    buf.resize(f->size());
    f->read_file(reinterpret_cast<std::uint8_t *>(&buf[0]), 1, static_cast<std::uint32_t>(buf.size()));

    f->close();

    common::ro_buf_stream stream(&buf[0], buf.size());
    loader::rsc_file test_rsc(reinterpret_cast<common::ro_stream *>(&stream));

    const std::uint16_t total_res = 3;
    REQUIRE(test_rsc.get_total_resources() == total_res);

    // Iterate through each resources
    for (int i = 1; i <= total_res; i++) {
        std::stringstream ss;
        ss << "loaderassets//SAMPLE_ROM_RESOURCE_DATA_IDX_";
        ss << i;
        ss << ".bin";

        std::ifstream fi(ss.str(), std::ios::ate | std::ios::binary);
        const std::size_t res_size = fi.tellg();

        std::vector<std::uint8_t> res_from_eka2l1 = test_rsc.read(i);
        fi.seekg(0, std::ios::beg);

        std::vector<std::uint8_t> expected_res;
        expected_res.resize(res_size);

        if (res_size > 0) {
            fi.read(reinterpret_cast<char *>(&expected_res[0]), res_size);
        }

        REQUIRE(res_from_eka2l1.size() == res_size);
        REQUIRE(expected_res == res_from_eka2l1);
    }
}

TEST_CASE("no_compress_but_may_contain_unicode_3", "rsc_file") {
    const char *rsc_name = "loaderassets//obscurersc.rsc";
    symfile f = eka2l1::physical_file_proxy(rsc_name, READ_MODE | BIN_MODE);

    REQUIRE(f);

    std::vector<std::uint8_t> buf;
    buf.resize(f->size());
    f->read_file(reinterpret_cast<std::uint8_t *>(&buf[0]), 1, static_cast<std::uint32_t>(buf.size()));

    f->close();

    common::ro_buf_stream stream(&buf[0], buf.size());
    loader::rsc_file test_rsc(reinterpret_cast<common::ro_stream *>(&stream));

    std::ifstream fi("loaderassets//obscurersc.seg1", std::ios::ate | std::ios::binary);
    const std::size_t res_size = fi.tellg();

    std::vector<std::uint8_t> res_from_eka2l1 = test_rsc.read(1);
    fi.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> expected_res;
    expected_res.resize(res_size);

    if (res_size > 0) {
        fi.read(reinterpret_cast<char *>(&expected_res[0]), res_size);
    }

    REQUIRE(res_from_eka2l1.size() == res_size);
    REQUIRE(expected_res == res_from_eka2l1);
}
