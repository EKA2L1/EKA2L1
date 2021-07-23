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
    using vfp_matcher = decoder::matcher<visitor, std::uint32_t>;

    template <typename V>
    std::optional<std::reference_wrapper<const vfp_matcher<V>>> decode_vfp(const std::uint32_t instruction) {
        using table_type = std::vector<vfp_matcher<V>>;

        static const struct tables_type {
            table_type unconditional;
            table_type conditional;
        } tables = [] {
            table_type list = {

#define INST(fn, name, bitstring) decoder::detail<vfp_matcher<V>>::get_matcher(&V::fn, name, bitstring),
#include <cpu/12l1r/encoding/vfp.inc>
#undef INST

            };

            const auto division = std::stable_partition(list.begin(), list.end(), [&](const auto &matcher) {
                return (matcher.get_mask() & 0xF0000000) == 0xF0000000;
            });

            return tables_type{
                table_type{ list.begin(), division },
                table_type{ division, list.end() },
            };
        }();

        const bool is_unconditional = (instruction & 0xF0000000) == 0xF0000000;
        const table_type &table = is_unconditional ? tables.unconditional : tables.conditional;

        const auto matches_instruction = [instruction](const auto &matcher) { return matcher.matches(instruction); };

        auto iter = std::find_if(table.begin(), table.end(), matches_instruction);
        return iter != table.end() ? std::optional<std::reference_wrapper<const vfp_matcher<V>>>(*iter) : std::nullopt;
    }
}
