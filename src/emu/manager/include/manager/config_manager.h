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
     * 
     * This also caches conversion value for fast speed.
    */
    struct config_node {
        /*! \brief A conversion cache of the lastest conversion
        */
        struct config_convert_cache {
            enum {
                cache_type_inv,
                cache_type_int
            } cache_type;
            
            union {
                std::int64_t intval;
            } val;

            config_convert_cache() 
                : cache_type(cache_type_inv) {
            }
        };

        std::vector<std::pair<std::string, config_convert_cache>> values;

        template <typename T, size_t IDX = 0>
        std::enable_if_t<std::is_same_v<T, std::string>> set(const std::string &val) {
            static_assert(IDX <= 50, "Config node can't store more than 50 values!");

            if (IDX >= values.size()) {
                values.resize(IDX + 1);
            }

            values[IDX].first = val;
            values[IDX].second.cache_type = config_convert_cache::cache_type_inv;
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

            return values[IDX].first;
        }

        template <typename T, size_t IDX = 0>
        std::enable_if_t<std::is_integral_v<T> || std::is_enum_v<T>, std::optional<T>> get() {
            if (values.size() <= IDX) {
                return std::nullopt;
            }
            
            if (values[IDX].second.cache_type == config_convert_cache::cache_type_int) {
                return static_cast<T>(values[IDX].second.val.intval);
            }

            std::string val = values[IDX].first;

            if (val == "false") {
                values[IDX].second.cache_type = config_convert_cache::cache_type_int;
                values[IDX].second.val.intval = static_cast<std::int64_t>(false);

                return static_cast<T>(false);
            } else if (val == "true") {
                values[IDX].second.cache_type = config_convert_cache::cache_type_int;
                values[IDX].second.val.intval = static_cast<std::int64_t>(true);

                return static_cast<T>(true);
            }

            const std::int64_t val_trans = std::atoll(val.c_str());

            values[IDX].second.cache_type = config_convert_cache::cache_type_int;
            values[IDX].second.val.intval = val_trans;

            return static_cast<T>(val_trans);
        }

        template <typename T, size_t IDX = 0>
        T get_or_fall(const T &def) {
            if (auto val = get<T, IDX>()) {
                return val.value();
            }

            return def;
        }
    };

    /*! \brief Base of config manager.
     *
     * The base implementation uses YAML to store and load configuration. Further
     * implementations can override deserialize/serialize method to provide new format.
    */
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
        virtual bool deserialize(const char *name);
        virtual bool serialize(const char *name);

        template <typename T, size_t IDX = 0>
        std::optional<T> get(const char *key) {
            auto find_res = nodes.find(key);

            if (find_res == nodes.end()) {
                return std::nullopt;
            }

            return find_res->second.get<T, IDX>();
        }
        
        template <typename T, size_t IDX = 0>
        T get_or_fall(const char *key, const T &def) {
            auto find_res = nodes.find(key);

            if (find_res == nodes.end()) {
                return def;
            }

            return find_res->second.get_or_fall<T, IDX>(def);
        }

        auto get_values(const char *key) -> std::vector<std::string> {
            auto find_res = nodes.find(key);

            if (find_res == nodes.end()) {
                return std::vector<std::string>{};
            }

            std::vector<std::string> results;
            for (auto &[value, cache]: find_res->second.values) {
                results.push_back(value);
            }

            return results;
        }

        template <typename T, size_t IDX = 0>
        void set(const char *key, const T &val) {
            nodes[key].set<T, IDX>(val);
        }
    };
}