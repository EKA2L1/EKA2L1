/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
 *
 * Original version of table by Lioncash.
 */

#pragma once

#include <cpu/12l1r/decoder/decoder_detail.h>
#include <cpu/12l1r/decoder/matcher.h>

#include <common/algorithm.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace eka2l1::arm::r12l1 {
    template <typename visitor>
    using arm_matcher = decoder::matcher<visitor, std::uint32_t>;

    template <typename V>
    std::vector<arm_matcher<V>> get_arm_decode_table() {
        std::vector<arm_matcher<V>> table = {
    #define INST(fn, name, bitstring) decoder::detail<arm_matcher<V>>::get_matcher(&V::fn, name, bitstring),
    #include <cpu/12l1r/encoding/arm.inc>
    #undef INST
        };

        // If a matcher has more bits in its mask it is more specific, so it should come first.
        std::stable_sort(table.begin(), table.end(), [](const auto& matcher1, const auto& matcher2) {
            return common::count_bit_set(matcher1.get_mask()) > common::count_bit_set(matcher2.get_mask());
        });

        return table;
    }

    template<typename V>
    std::optional<std::reference_wrapper<const arm_matcher<V>>> decode_arm(const std::uint32_t instruction) {
        static const auto table = get_arm_decode_table<V>();
        const auto matches_instruction = [instruction](const auto& matcher) { return matcher.matches(instruction); };

        auto iter = std::find_if(table.begin(), table.end(), matches_instruction);
        return iter != table.end() ? std::optional<std::reference_wrapper<const arm_matcher<V>>>(*iter) : std::nullopt;
    }
}
