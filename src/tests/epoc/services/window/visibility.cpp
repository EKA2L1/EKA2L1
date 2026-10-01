/*
 * Copyright (c) 2026 EKA2L1 Team.
 * This file is part of EKA2L1, licensed under GPL version 3 or later.
 */

#include <catch2/catch.hpp>
#include <services/window/classes/winbase.h>

TEST_CASE("A child created under a hidden parent keeps its own visibility", "[window]") {
    using eka2l1::epoc::window;
    window parent(nullptr, nullptr, nullptr);
    parent.priority = 0;
    parent.flags &= ~window::flags_visible;

    window child(nullptr, nullptr, &parent);
    child.priority = 0;
    child.set_initial_state();
    REQUIRE((child.flags & window::flags_visible) != 0);

    parent.flags |= window::flags_visible;
    REQUIRE((child.flags & window::flags_visible) != 0);
    child.remove_from_sibling_list();
}
