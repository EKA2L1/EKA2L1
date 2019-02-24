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

#include <common/chunkyseri.h>

#include <epoc/loader/spi.h>
#include <epoc/vfs.h>

TEST_CASE("normal_spi_file_read", "spi_file") {
    const char *spi_name = "loaderassets//ecom-1-0.spi";
    eka2l1::symfile f = eka2l1::physical_file_proxy(spi_name, READ_MODE | BIN_MODE);

    REQUIRE(f);

    std::vector<std::uint8_t> buf;
    buf.resize(f->size());
    f->read_file(reinterpret_cast<std::uint8_t *>(&buf[0]), 1, static_cast<std::uint32_t>(buf.size()));

    f->close();

    // Inistantiate serializer
    eka2l1::common::chunkyseri seri(&buf[0], buf.size(), eka2l1::common::SERI_MODE_READ);
    eka2l1::loader::spi_file spi(0);

    bool result = spi.do_state(seri);

    REQUIRE(result);

    REQUIRE(spi.entries.size() == 1);
    REQUIRE(spi.entries[0].name == "emailobservermtmplugin");
}
