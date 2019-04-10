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
#include <epoc/loader/mif.h>

#include <common/buffer.h>
#include <epoc/vfs.h>

using namespace eka2l1;

TEST_CASE("simple_icon_handler", "mif_file") {
    const char *rsc_name = "loaderassets//itried.mif";
    symfile f = eka2l1::physical_file_proxy(rsc_name, READ_MODE | BIN_MODE);

    REQUIRE(f);

    eka2l1::ro_file_stream stream(f);
    loader::mif_file miff(reinterpret_cast<common::ro_stream*>(&stream));

    miff.do_parse();

    int dest_size = 0;
    miff.read_mif_entry(0, nullptr, dest_size);

    std::vector<std::uint8_t> buf;
    buf.resize(dest_size);

    miff.read_mif_entry(0, &buf[0], dest_size);

    loader::mif_icon_header *header = reinterpret_cast<decltype(header)>(&buf[0]);

    REQUIRE(header->type == 1);
    REQUIRE(header->data_offset == sizeof(loader::mif_icon_header));
}
