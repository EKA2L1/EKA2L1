/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

/* This ini parser is made to deal with many INI types across Symbian OS
 *
 * There includes wsini, cenrep ini, etc.. These INI are advances from the original INI 
 * format and each server has it own parser.
 * 
 * Here i group them all, and keep struct as lightweight as possible.
 * 
 * Parsing support
 * Two types of prop (pair):
 * 
 * INT A B (Prop seperate with space)
 * A=5, 7, 9 (Prop equals + seperate with comma)
 * 
 * These can be recursive, but don't test it to its limit:
 * 
 * INT A = 5, 7, 9 B
 * 
 * Will be as:
 * PROP KEY = INT, VALUES = { {KEY = A, VALUE = {5, 7, 9} } , B }
*/

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace eka2l1::common {
    enum ini_node_type {
        INI_NODE_INVALID,
        INI_NODE_VALUE,
        INI_NODE_KEY,
        INI_NODE_PAIR,
        INI_NODE_SECTION
    };

    class ini_node {
    protected:
        ini_node_type node_type;

        ini_node(const ini_node_type node_type)
            : node_type(node_type) {
        }

    public:
        ini_node()
            : node_type(INI_NODE_INVALID) {
        }

        virtual const char *name() = 0;

        ini_node_type get_node_type() const {
            return node_type;
        }

        template <typename T>
        T *get_as() {
            return (reinterpret_cast<T *>(this));
        }
    };

    using ini_node_ptr = std::shared_ptr<ini_node>;

    class ini_value : public ini_node {
        friend class ini_file;
        friend class ini_section;

    protected:
        std::string value;

        ini_value(const ini_node_type t)
            : ini_node(t) {
        }

    public:
        ini_value()
            : ini_node(INI_NODE_VALUE) {
        }

        ini_value(const std::string &val)
            : ini_node(INI_NODE_VALUE)
            , value(val) {
        }

        template <typename T>
        std::enable_if_t<std::is_integral_v<T>, T> get_as_native() {
            std::string vc = value;

            if (vc.length() > 2 && vc[0] == '0' && (vc[1] == 'x' || vc[1] == 'X')) {
                vc = vc.substr(2, vc.length() - 2);
                return static_cast<T>(std::strtoll(vc.c_str(), nullptr, 16));
            }

            return static_cast<T>(std::atoll(vc.c_str()));
        }

        template <typename T>
        std::enable_if_t<std::is_floating_point_v<T>, T> get_as_native() {
            return static_cast<T>(std::stod(value));
        }

        virtual const char *name() override {
            return value.c_str();
        }

        std::string get_value() {
            return value;
        }
    };

    using ini_value_node_ptr = std::shared_ptr<ini_value>;

    class ini_pair : public ini_node {
        friend class ini_section;
        friend class ini_file;

        std::string key;
        std::vector<ini_node_ptr> values;

    public:
        ini_pair()
            : ini_node(INI_NODE_PAIR) {
        }

        std::size_t get_value_count() {
            return values.size();
        }

        const char *name() override {
            return key.c_str();
        }

        struct iterator {
            ini_pair *parent;
            std::size_t my_index;

            iterator operator++() {
                ++my_index;
                return *this;
            }

            bool operator!=(const iterator &other) const {
                return my_index != other.my_index;
            }

            common::ini_node_ptr &operator*() {
                return parent->values[my_index];
            }
        };

        iterator begin() {
            return iterator{ this, 0 };
        }

        iterator end() {
            return iterator{ this, values.size() };
        }

        ini_node_ptr operator[](const std::size_t idx) {
            // No check, expect to throw
            return values[idx];
        }

        bool set(std::uint32_t *vals, int count);
        bool set(bool *vals, int count);
        bool set(std::vector<std::string> &vals);

        std::size_t get(std::uint32_t *val, int count, std::uint32_t default_val = 0);
        std::size_t get(bool *val, int count, bool default_val = true);
        std::size_t get(std::vector<std::string> &val);

        template <typename T>
        T *get(const std::size_t idx) {
            if (idx < values.size()) {
                return values[idx]->get_as<T>();
            }

            return nullptr;
        }

        template <typename T>
        std::enable_if_t<std::is_integral_v<T>, T> key_as() {
            std::string vc = key;

            if (vc.length() > 2 && vc[0] == '0' && (vc[1] == 'x' || vc[1] == 'X')) {
                vc = vc.substr(2, vc.length() - 2);
                return static_cast<T>(std::strtoll(vc.c_str(), nullptr, 16));
            }

            return static_cast<T>(std::atoll(vc.c_str()));
        }

        template <typename T>
        std::enable_if_t<std::is_floating_point_v<T>, T> key_as() {
            return static_cast<T>(std::stod(key));
        }

        static bool is_my_type(const ini_node_ptr node) {
            return (node->get_node_type() == INI_NODE_PAIR);
        }
    };

    class ini_section : public ini_node {
    protected:
        friend class ini_file;

        std::string sec_name;
        std::vector<ini_node_ptr> nodes;

    public:
        ini_section()
            : ini_node(INI_NODE_SECTION) {
        }

        const char *name() override {
            return sec_name.c_str();
        }

        bool node_exists(const char *name);

        struct iterator {
            ini_section *parent;
            std::size_t my_index;

            iterator operator++() {
                ++my_index;
                return *this;
            }

            bool operator!=(const iterator &other) const {
                return my_index != other.my_index;
            }

            common::ini_node_ptr &operator*() {
                return parent->nodes[my_index];
            }
        };

        iterator begin() {
            return iterator{ this, 0 };
        }

        iterator end() {
            return iterator{ this, nodes.size() };
        }

        /* \brief Access a node with given name/key.
        */
        ini_node_ptr operator[](const char *name);
        ini_node_ptr find(const char *name);
        ini_node_ptr find_ignore_case(const char *name);

        ini_section *create_section(const char *name);

        // Note that an empty key will still be saved
        ini_pair *create_pair(const char *key);

        ini_value *create_value(const char *val);

        std::size_t get(const char *key,
            std::uint32_t *val, int count, std::uint32_t default_val = 0, int *error_code = nullptr);

        std::size_t get(const char *key,
            bool *val, int count, bool default_val = 0, int *error_code = nullptr);

        std::size_t get(const char *key, std::vector<std::string> &val, int *error_code = nullptr);

        static bool is_my_type(const ini_node_ptr node) {
            return (node->get_node_type() == INI_NODE_SECTION);
        }

        template <typename T>
        T *get(const std::size_t idx) {
            if (idx < nodes.size()) {
                return nodes[idx]->get_as<T>();
            }

            return nullptr;
        }
    };

    class ini_file : public ini_section {
    public:
        /*! \brief Load an INI file given a path
         * \returns 0 if success.
         *         -1 if file not found
         *         -2 if file is invalid (syntax invalid).
        */
        int load(const char *path, bool ignore_spaces = true);

        /*! \brief Save an ini file
        */
        // void save(const char *path);
    };
}