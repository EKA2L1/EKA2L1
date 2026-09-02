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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <catch2/catch.hpp>

#include <utils/obj.h>

#include <vector>

using namespace eka2l1;

namespace {
    struct counted_object : public epoc::ref_count_object {
        std::uint32_t marker = 0;
    };

    struct recording_container : public epoc::object_container {
        std::vector<epoc::ref_count_object *> removed;

        void clear() override {
        }

        bool remove(epoc::ref_count_object *obj) override {
            removed.push_back(obj);
            return true;
        }
    };
}

TEST_CASE("object_table_refuses_a_handle_it_no_longer_holds", "utils") {
    recording_container container;

    counted_object first;
    counted_object second;
    first.owner = &container;
    second.owner = &container;
    first.marker = 1;
    second.marker = 2;

    epoc::object_table table;

    const epoc::handle first_handle = table.add(&first);
    const epoc::handle second_handle = table.add(&second);
    REQUIRE(first_handle != second_handle);

    REQUIRE(table.get<counted_object>(first_handle) == &first);
    REQUIRE(table.remove(first_handle));

    REQUIRE(table.get<counted_object>(first_handle) == nullptr);
    REQUIRE_FALSE(table.remove(first_handle));

    REQUIRE(table.get<counted_object>(second_handle) == &second);
    REQUIRE(table.get<counted_object>(second_handle)->marker == 2);
}

TEST_CASE("object_table_refuses_handles_that_never_existed", "utils") {
    recording_container container;

    counted_object object;
    object.owner = &container;

    epoc::object_table table;
    const epoc::handle valid = table.add(&object);

    REQUIRE_FALSE(table.remove(0));
    REQUIRE_FALSE(table.remove(valid + 0x10));
    REQUIRE(table.get<counted_object>(valid) == &object);
}
