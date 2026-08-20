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

#include <services/framework.h>

using namespace eka2l1;

namespace {
    struct counted_object : public epoc::ref_count_object {
        std::uint32_t marker = 0;
    };
}

TEST_CASE("object_container_answers_only_for_ids_it_owns", "service_framework") {
    service::normal_object_container container;

    counted_object *first = container.make_new<counted_object>();
    counted_object *second = container.make_new<counted_object>();
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    first->marker = 1;
    second->marker = 2;

    const service::uid first_id = first->id;
    const service::uid second_id = second->id;
    REQUIRE(first_id != second_id);

    REQUIRE(container.get<counted_object>(first_id) == first);
    REQUIRE(container.get<counted_object>(second_id) == second);

    // A handle the client has already closed. The lookup used to land on the next
    // object in id order and hand that back, so the caller kept working with a live
    // object of possibly another type instead of seeing the handle go away.
    REQUIRE(container.remove(first));
    REQUIRE(container.get<counted_object>(first_id) == nullptr);
    REQUIRE(container.get<counted_object>(second_id) == second);
    REQUIRE(container.get<counted_object>(second_id)->marker == 2);

    // Zero is the handle a command uses to say it has no object, and ids past the
    // end never existed. Neither may resolve to anything.
    REQUIRE(container.get<counted_object>(0) == nullptr);
    REQUIRE(container.get<counted_object>(second_id + 100) == nullptr);
}
