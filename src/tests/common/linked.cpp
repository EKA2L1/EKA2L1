/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <catch2/catch.hpp>
#include <common/linked.h>

using namespace eka2l1;

TEST_CASE("single roundabout member is linked", "linked") {
    common::roundabout queue;
    common::double_linked_queue_element member;

    REQUIRE(member.alone());

    queue.push(&member);

    REQUIRE_FALSE(member.alone());
    REQUIRE(queue.first() == &member);
    REQUIRE(queue.last() == &member);

    member.deque();

    REQUIRE(member.alone());
    REQUIRE(queue.empty());
}

TEST_CASE("roundabout_reset_leaves_the_members_untouched", "linked") {
    common::roundabout queue;
    common::double_linked_queue_element member;

    queue.push(&member);
    queue.reset();

    // The ring is empty, and the member still believes it is enqueued: reset()
    // exists for the case where the members are already dead memory.
    REQUIRE(queue.empty());
    REQUIRE_FALSE(member.alone());
}
