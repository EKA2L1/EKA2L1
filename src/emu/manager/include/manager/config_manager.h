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

#pragma once

#include <vector>
#include <unordered_map>
#include <optional>

namespace eka2l1::manager {
    /*! \brief A struct represents a config node
     *
     * A config can be a list, a single bool, or more. Considers this as
     * a variant, but lightweight and for config manager.
    */
    struct config_node {
        std::vector<std::string> values;

        template <typename T, size_t IDX = 0>
        std::enable_if_t<std::is_same_v<T, std::string>> set(const std::string &val) {
            static_assert(IDX <= 50, "Config node can't store more than 50 values!");

            if (IDX >= values.size()) {
                values.resize(IDX + 1);
            }

            values[IDX] = val;
        }

        template <typename T, size_t IDX = 0>
        std::enable_if_t<std::is_integral_v<T>> set(const T &value) {
            set<T, IDX>(std::to_string(value));
        }
        
        template <typename T, size_t IDX = 0>
        std::enable_if_t<std::is_enum_v<T>> set(const T &value) {
            set<T, IDX>(std::to_string(static_cast<int>(value)));
        }

        template <typename T, size_t IDX = 0>
        std::enable_if_t<std::is_same_v<T, std::string>, std::optional<T>> get() {
            if (values.size() <= IDX) {
                return std::nullopt;
            }

            return values[IDX];
        }

        template <typename T, size_t IDX = 0>
        std::enable_if_t<std::is_integral_v<T> || std::is_enum_v<T>, std::optional<T>> get() {
            if (values.size() <= IDX) {
                return std::nullopt;
            }
            
            std::string val = values[IDX];
            const std::int64_t val_trans = std::atoll(val.c_str());

            return static_cast<T>(val_trans);
        }

        template <typename T, size_t IDX = 0>
        T get_or_fall(const T &def) {
            if (auto val = get<T, IDX>()) {
                return val;
            }

            return def;
        }
    };

    class config_manager {
        std::unordered_map<std::string, config_node> nodes;

    public:
        explicit config_manager() = default;
        ~config_manager();

        /*! \brief Load and store nodes to config manager from a file.
         *
         * \param name Name of the file to load config from.
         * \returns True on success.
        */
        bool deserialize(const char *name);
        bool serialize(const char *name);

        template <typename T, size_t IDX = 0>
        std::optional<T> get(const char *key) {
            auto find_res = nodes.find(key);

            if (find_res == nodes.end()) {
                return std::nullopt;
            }

            return find_res->second.get<T, IDX>();
        }
        
        template <typename T, size_t IDX = 0>
        std::optional<T> get_or_fall(const char *key, const T &def) {
            auto find_res = nodes.find(key);

            if (find_res == nodes.end()) {
                return std::nullopt;
            }

            return find_res->second.get_or_fall<T, IDX>(def);
        }

        template <typename T, size_t IDX = 0>
        void set(const char *key, const T &val) {
            nodes[key].set<T, IDX>(val);
        }
    };
}