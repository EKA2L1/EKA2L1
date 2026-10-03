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

#include <control/dispatcher.h>
#include <control/params.h>

#include <cctype>
#include <cerrno>
#include <cstdlib>

namespace eka2l1::control {
    static rpc_error invalid(const char *name, const std::string &what) {
        return rpc_error(error_invalid_params, std::string("Parameter '") + name + "' " + what);
    }

    static const rapidjson::Value *find(const rapidjson::Value &params, const char *name) {
        const rapidjson::Value::ConstMemberIterator member = params.FindMember(name);
        return (member == params.MemberEnd()) ? nullptr : &member->value;
    }

    std::optional<std::string> optional_string(const rapidjson::Value &params, const char *name) {
        const rapidjson::Value *value = find(params, name);

        if (!value) {
            return std::nullopt;
        }

        if (!value->IsString()) {
            throw invalid(name, "must be a string");
        }

        return std::string(value->GetString(), value->GetStringLength());
    }

    std::string require_string(const rapidjson::Value &params, const char *name) {
        std::optional<std::string> value = optional_string(params, name);

        if (!value) {
            throw invalid(name, "is required");
        }

        return *value;
    }

    std::optional<std::int64_t> optional_integer(const rapidjson::Value &params, const char *name, const std::int64_t min, const std::int64_t max) {
        const rapidjson::Value *value = find(params, name);

        if (!value) {
            return std::nullopt;
        }

        if (!value->IsInt64()) {
            throw invalid(name, "must be an integer");
        }

        const std::int64_t number = value->GetInt64();

        if ((number < min) || (number > max)) {
            throw invalid(name, "must be between " + std::to_string(min) + " and " + std::to_string(max));
        }

        return number;
    }

    std::int64_t require_integer(const rapidjson::Value &params, const char *name, const std::int64_t min, const std::int64_t max) {
        std::optional<std::int64_t> value = optional_integer(params, name, min, max);

        if (!value) {
            throw invalid(name, "is required");
        }

        return *value;
    }

    std::uint32_t require_uid(const rapidjson::Value &params, const char *name) {
        const rapidjson::Value *value = find(params, name);

        if (!value) {
            throw invalid(name, "is required");
        }

        if (value->IsString()) {
            const std::string text(value->GetString(), value->GetStringLength());

            if ((text.size() < 3) || (text.size() > 10) || (text[0] != '0') || ((text[1] != 'x') && (text[1] != 'X'))
                || !std::isxdigit(static_cast<unsigned char>(text[2]))) {
                throw invalid(name, "must be a number or a hexadecimal string such as \"0xE7351C2F\"");
            }

            char *end = nullptr;
            errno = 0;
            const unsigned long long uid = std::strtoull(text.c_str() + 2, &end, 16);

            if ((errno != 0) || (*end != '\0') || (uid > 0xFFFFFFFFULL)) {
                throw invalid(name, "must be a number or a hexadecimal string such as \"0xE7351C2F\"");
            }

            return static_cast<std::uint32_t>(uid);
        }

        return static_cast<std::uint32_t>(require_integer(params, name, 0, 0xFFFFFFFFLL));
    }
}
