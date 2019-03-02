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
#include <epoc/loader/mbm.h>

#include <common/buffer.h>

#include <fstream>
#include <sstream>
#include <vector>

using namespace eka2l1;

/**
 * Try to read headers from a MBM file. Test its results. 
 */
TEST_CASE("mbm_header_trailer_and_single_headers", "mbm_file") {
    std::ifstream fi("loaderassets\\face.mbm", std::ios::binary);
    REQUIRE(!fi.bad());

    std::vector<std::uint8_t> data;

    fi.seekg(0, std::ios::end);
    const std::uint64_t fsize = fi.tellg();

    data.resize(fsize);
    fi.seekg(0, std::ios::beg);

    fi.read(reinterpret_cast<char*>(&data[0]), data.size());

    common::ro_buf_stream stream(&data[0], data.size());
    loader::mbm_file mbmf(stream);

    REQUIRE(mbmf.do_read_headers());
    REQUIRE(mbmf.trailer.count == 1);
    REQUIRE(mbmf.trailer.sbm_offsets[0] == 20);

    REQUIRE(mbmf.sbm_headers[0].compressed_len == 3545);
    REQUIRE(mbmf.sbm_headers[0].bit_per_pixels == 24);
}
