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

#include <manager/config_manager.h>
#include <yaml-cpp/yaml.h>

#include <cassert>

namespace eka2l1::manager {
    config_manager::~config_manager() {
    }

    bool config_manager::serialize(const char *name) {
        return true;
    }

    bool config_manager::deserialize(const char *name) {
        YAML::Node root;

        try {
            root = YAML::LoadFile(name);
        } catch (YAML::Exception &exception) {
            return false;
        }

        for (auto node: root) {
            if (!node.IsMap()) {
                continue;
            }

            const std::string key = node.first.as<std::string>();
            config_node cfg_node;

            if (node.second.IsScalar()) {
                cfg_node.values.push_back(node.second.as<std::string>());
            } else {
                for (auto subnode: node.second) {
                    if (subnode.IsScalar()) {
                        cfg_node.values.push_back(subnode.as<std::string>());
                    }
                }
            }

            nodes.emplace(key, std::move(cfg_node));
        }

        return true;
    }
}