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

#include <core/epoc/panic.h>
#include <yaml-cpp/yaml.h>

#include <mutex>

namespace eka2l1::epoc {
    YAML::Node panic_node;
    std::mutex panic_mut;

    bool init_panic_descriptions() {
        std::lock_guard<std::mutex> guard(panic_mut);

        panic_node.reset();

        try {
            panic_node = YAML::LoadFile("panic.json")["Panic"];
        } catch (...) {
            return false;
        }

        return true;
    }

    bool is_panic_category_action_default(const std::string &panic_category) {
        try {
            const std::string action = panic_node[panic_category]["action"].as<std::string>();

            if (action == "script") {
                return false;
            }
        } catch (...) {
            return true;
        }

        return true;
    }

    std::optional<std::string> get_panic_description(const std::string &category, const int code) {
        try {
            const std::string description = panic_node[category][code].as<std::string>();
            return description;
        } catch (...) {
            return std::optional<std::string>{};
        }

        return std::optional<std::string>{};
    }
}