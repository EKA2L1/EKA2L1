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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <rapidjson/document.h>

#include <cstdint>
#include <optional>
#include <string>

// Readers for named method parameters. Each one throws rpc_error(error_invalid_params)
// naming the parameter when it is missing (for require_*) or has the wrong type.
namespace eka2l1::control {
    std::string require_string(const rapidjson::Value &params, const char *name);
    std::optional<std::string> optional_string(const rapidjson::Value &params, const char *name);

    std::int64_t require_integer(const rapidjson::Value &params, const char *name, const std::int64_t min, const std::int64_t max);
    std::optional<std::int64_t> optional_integer(const rapidjson::Value &params, const char *name, const std::int64_t min, const std::int64_t max);

    /**
     * \brief Read a Symbian UID: a number, or a string holding a hexadecimal number ("0xE7351C2F").
     */
    std::uint32_t require_uid(const rapidjson::Value &params, const char *name);
}
