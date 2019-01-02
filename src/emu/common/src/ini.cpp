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

#include <common/ini.h>
#include <common/dynamicfile.h>
#include <common/cvt.h>

#include <cstring>
#include <fstream>
#include <sstream>

namespace eka2l1::common {
    ini_node_ptr ini_section::find(const char *name) {
        const std::size_t len = strlen(name);

        for (auto node: nodes) {
            if (strncmp(name, node->name(), len) == 0) {
                return node;
            }
        }

        return nullptr;
    }

    ini_node_ptr ini_section::operator[](const char *name) {
        return find(name);
    }

    bool ini_section::node_exists(const char *name) {
        const std::size_t len = strlen(name);

        for (auto node: nodes) {
            if (strncmp(name, node->name(), len) == 0) {
                return true;
            }
        }

        return false;
    }
    
    ini_section *ini_section::create_section(const char *name) {
        if (node_exists(name)) {
            return nullptr;
        }

        std::shared_ptr<ini_section> section = std::make_shared<ini_section>();
        section->sec_name = name;

        ini_section *new_sec = &(*section);
        nodes.push_back(std::move(section));
        
        return new_sec;
    }

    // Note that an empty key will still be saved 
    ini_pair *ini_section::create_pair(const char *key) {
        if (node_exists(key)) {
            return nullptr;
        }

        std::shared_ptr<ini_pair> pair = std::make_shared<ini_pair>();
        pair->key = key;

        ini_pair *new_pair = &(*pair);
        nodes.push_back(std::move(pair));
        
        return new_pair;
    }

    std::size_t ini_section::get(const char *key, 
        std::uint32_t *val, int count, std::uint32_t default_val, int *error_code) {        
        ini_node_ptr node = find(key);

        if (!node) {
            error_code ? (*error_code = -1) : 0;
            return false;
        }

        std::shared_ptr<ini_pair> pair = std::reinterpret_pointer_cast<ini_pair>(node);

        error_code ? (*error_code= 0) : 0;
        return pair->get(val, count, default_val);
    }

    std::size_t ini_section::get(const char *key, 
        bool *val, int count, bool default_val, int *error_code) {
            ini_node_ptr node = find(key);

        if (!node) {
            error_code ? (*error_code = -1) : 0;
            return false;
        }

        std::shared_ptr<ini_pair> pair = std::reinterpret_pointer_cast<ini_pair>(node);

        error_code ? (*error_code= 0) : 0;
        return pair->get(val, count, default_val);
    }

    std::size_t ini_section::get(const char *key, std::vector<std::string> &val, int *error_code) {
        ini_node_ptr node = find(key);

        if (!node) {
            error_code ? (*error_code = -1) : 0;
            return false;
        }

        std::shared_ptr<ini_pair> pair = std::reinterpret_pointer_cast<ini_pair>(node);

        error_code ? (*error_code= 0) : 0;
        return pair->get(val);
    }

    bool ini_pair::set(std::uint32_t *vals, int count) {
        if (count <= 0) {
            return false;
        }

        values.resize(count);

        for (int i = 0; i < count; i++) {
            values[i] = std::to_string(vals[i]);
        }

        return true;
    }

    bool ini_pair::set(bool *vals, int count) {
        if (count <= 0) {
            return false;
        }

        values.resize(count);

        for (int i = 0; i < count; i++) {
            values[i] = (vals[i]) ? "1" : "0";
        }

        return true;
    }

    bool ini_pair::set(std::vector<std::string> &vals) {
        values.assign(vals.begin(), vals.end());
        return true;
    }
    
    std::size_t ini_pair::get(std::uint32_t *val, int count, std::uint32_t default_val) {
        std::size_t success_translation = 0;
        values.resize(count);

        for (std::size_t i = 0; i < values.size(); i++) {
            try {
                *(val + i) = std::atol(values[i].c_str());
                success_translation++;
            } catch (...) {
                *(val + i) = default_val;
            }
        }

        return success_translation;
    }

    std::size_t ini_pair::get(bool *val, int count, bool default_val) {
        std::size_t success_translation = 0;
        values.resize(count);

        for (std::size_t i = 0; i < values.size(); i++) {
            try {
                *(val + i) = static_cast<bool>(std::atoi(values[i].c_str()));
                success_translation++;
            } catch (...) {
                *(val + i) = default_val;
            }
        }

        return success_translation;
    }

    std::size_t ini_pair::get(std::vector<std::string> &val) {
        val.assign(values.begin(), values.end());
        return values.size();
    }

    struct ini_linestream {
        std::string line;
        int counter;

        explicit ini_linestream(const std::string &l)
            : line(l), counter(0) {
            if (line.back() == '\r') {
                line.erase(line.length() - 1, 1);
            }
        }

        std::string next_string() {
            while (counter < line.length() && line[counter] == ' ') {
                counter++;
            }

            if (counter >= line.length()) {
                return "";
            }

            char cto_stop = ' ';

            if (line[counter] == '"') {
                cto_stop = '"';
                counter += 1;
            }

            std::size_t begin = counter;

            while (counter < line.length() && line[counter] != cto_stop) {
                counter++;
            }

            std::size_t len = counter - begin - (cto_stop == '"' ? 1 : 0);
            return line.substr(begin, len);
        }

        bool eof() {
            return counter >= line.length();
        }
    };

    int ini_file::load(const char *path){
        common::dynamic_ifile ifile(path);

        if (ifile.fail()) {
            return -1;
        }

        ini_section *sec = reinterpret_cast<ini_section*>(this);
        std::string line;

        while (ifile.getline(line)) {
            ini_linestream stream(line);
            std::string first_token = stream.next_string();

            if ((!first_token.empty()) && (first_token[0] != ';') && (first_token.substr(0, 2) != "//")) {
                if (first_token[0] == '[') {
                    first_token = first_token.substr(1, first_token.length() - 2);

                    if (first_token == "") {
                        return -2;
                    }

                    sec = create_section(first_token.c_str());
                } else {
                    ini_pair *pair = sec->create_pair(first_token.c_str());
                    
                    while (!stream.eof()) {
                        pair->values.push_back(stream.next_string());
                    }
                }
            }
        }

        return 0;
    }
}