/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
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
    using thumb32_matcher = decoder::matcher<visitor, std::uint32_t>;

    template <typename V>
    std::optional<std::reference_wrapper<const thumb32_matcher<V>>> decode_thumb32(const std::uint32_t instruction) {
        static const std::vector<thumb32_matcher<V>> table = {
#define INST(fn, name, bitstring) decoder::detail<thumb32_matcher<V>>::get_matcher(&V::fn, name, bitstring)
#include <cpu/12l1r/encoding/thumb32.inc>
#undef INST
        };

        const auto matches_instruction = [instruction](const auto &matcher) { return matcher.matches(instruction); };

        auto iter = std::find_if(table.begin(), table.end(), matches_instruction);
        return iter != table.end() ? std::optional<std::reference_wrapper<const thumb32_matcher<V>>>(*iter) : std::nullopt;
    }
}
