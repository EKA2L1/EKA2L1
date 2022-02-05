/*
 * Copyright (c) 2022 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 projec.
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

#pragma once

#include <cstdint>
#include <functional>

#include <common/types.h>

namespace eka2l1 {
    namespace loader {
        struct sis_controller;
        struct sis_registry_tree;

        using show_text_func = std::function<bool(const char *, const bool)>;
        using choose_lang_func = std::function<int(const int *langs, const int count)>;
        using var_value_resolver_func = std::function<std::int32_t(std::uint32_t)>;
    }
}