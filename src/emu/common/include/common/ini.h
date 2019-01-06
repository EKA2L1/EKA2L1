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

#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

namespace eka2l1::common {
    enum ini_node_type {
        INI_NODE_INVALID,
        INI_NODE_PAIR,
        INI_NODE_SECTION
    };

    class ini_node {
    protected:
        ini_node_type node_type;

        ini_node(const ini_node_type node_type)
            : node_type(node_type)
        {
            
        }

    public:
        ini_node()
            : node_type(INI_NODE_INVALID) 
        {

        }

        virtual const char *name() = 0;

        ini_node_type get_node_type() const {
            return node_type;
        }

        template <typename T>
        T *get_as() {
            return (reinterpret_cast<T*>(this));
        }
    };

    using ini_node_ptr = std::shared_ptr<ini_node>;

    class ini_pair: public ini_node {
        friend class ini_section;
        friend class ini_file;

        std::string key;
        std::vector<std::string> values;

    public:
        ini_pair()
            : ini_node(INI_NODE_PAIR)
        {
            
        }

        std::size_t get_value_count() {
            return values.size();
        }

        const char *name() override {
            return key.c_str();
        }

        bool set(std::uint32_t *vals, int count);
        bool set(bool *vals, int count);
        bool set(std::vector<std::string> &vals);

        std::size_t get(std::uint32_t *val, int count, std::uint32_t default_val = 0);
        std::size_t get(bool *val, int count, bool default_val = true);
        std::size_t get(std::vector<std::string> &val);

        static bool is_my_type(const ini_node_ptr node) {
            return (node->get_node_type() == INI_NODE_PAIR);
        }
    };

    class ini_section : public ini_node {
    protected:
        friend class ini_file;

        std::string               sec_name;
        std::vector<ini_node_ptr> nodes;

    public:
        ini_section()
            : ini_node(INI_NODE_SECTION)
        {

        }

        const char *name() override {
            return sec_name.c_str();
        }
        
        bool node_exists(const char *name);

        /* \brief Access a node with given name/key.
        */
        ini_node_ptr operator[](const char *name);
        ini_node_ptr find(const char *name);

        ini_section *create_section(const char *name);

        // Note that an empty key will still be saved 
        ini_pair *create_pair(const char *key);

        std::size_t get(const char *key, 
            std::uint32_t *val, int count, std::uint32_t default_val = 0, int *error_code = nullptr);

        std::size_t get(const char *key, 
            bool *val, int count, bool default_val = 0, int *error_code = nullptr);

        std::size_t get(const char *key, std::vector<std::string> &val, int *error_code = nullptr);

        static bool is_my_type(const ini_node_ptr node) {
            return (node->get_node_type() == INI_NODE_SECTION);
        }
    };
    
    class ini_file: public ini_section {
    public:
        /*! \brief Load an INI file given a path
         * \returns 0 if success.
         *         -1 if file not found
         *         -2 if file is invalid (syntax invalid).
        */
        int load(const char *path);

        /*! \brief Save an ini file
        */
        // void save(const char *path);
    };
}