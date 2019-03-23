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

#include <catch2/catch.hpp>
#include <common/algorithm.h>

using namespace eka2l1;

template <typename T>
void detect_change_vector(const std::vector<T> &old_, const std::vector<T> &new_, std::vector<T> &added,
    std::vector<T> &removed) {
    common::addition_callback_func add_cb = [&](const std::size_t idx) {
        added.push_back(new_[idx]);
    };

    common::remove_callback_func rev_cb = [&](const std::size_t idx) {
        removed.push_back(old_[idx]);
    };

    common::detect_changes(old_, new_, add_cb, rev_cb);
} 

TEST_CASE("detect_equal_comp", "detect_changes") {
    std::vector<int> v1 = { 1, 2, 4, 5, 7 };
    std::vector<int> v2 = { 2, 3, 4, 5, 9 };

    std::vector<int> removes;
    std::vector<int> added;

    detect_change_vector(v1, v2, added, removes);

    REQUIRE(removes == std::vector<int>({ 1, 7 }));
    REQUIRE(added == std::vector<int>({ 3, 9 }));
}

TEST_CASE("detect_diff_comp_old_bigger", "detect_changes") {
    std::vector<int> v1 = { 1, 2, 4, 5, 7, 11, 15 };
    std::vector<int> v2 = { 2, 3, 4, 5, 9 };

    std::vector<int> removes;
    std::vector<int> added;

    detect_change_vector(v1, v2, added, removes);

    REQUIRE(removes == std::vector<int>({ 1, 7, 11, 15 }));
    REQUIRE(added == std::vector<int>({ 3, 9 }));
}

TEST_CASE("detect_diff_comp_new_bigger", "detect_changes") {
    std::vector<int> v1 = { 1, 2, 4, 5, 7 };
    std::vector<int> v2 = { 2, 3, 4, 5, 9, 13, 20 };

    std::vector<int> removes;
    std::vector<int> added;

    detect_change_vector(v1, v2, added, removes);

    REQUIRE(removes == std::vector<int>({ 1, 7 }));
    REQUIRE(added == std::vector<int>({ 3, 9, 13, 20 }));
}
