/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <config/panic_blacklist.h>
#include <yaml-cpp/yaml.h>

#include <common/log.h>
#include <common/buffer.h>

namespace eka2l1::config {
    panic_blacklist::panic_blacklist() {
        try {
            YAML::Node the_node;

            common::ro_std_file_stream blacklist_stream("compat//panicBlackList.json", true);
            if (!blacklist_stream.valid()) {
                LOG_ERROR(CONFIG, "Failed to open thread panic blacklist file!");
                return;
            } else {
                std::string whole_config(blacklist_stream.size(), ' ');
                blacklist_stream.read(whole_config.data(), whole_config.size());

                the_node = YAML::Load(whole_config);
            }

            for (auto iterator_val: the_node) {
                const std::string process_name = iterator_val.first.as<std::string>();
                for (auto smaller_iterator: iterator_val.second) {
                    panic_block_info info;

                    info.process_name_ = process_name;
                    info.thread_name_ = smaller_iterator.first.as<std::string>();
                    info.category_ = smaller_iterator.second["category"].as<std::string>();
                    info.code_ = smaller_iterator.second["code"].as<std::int32_t>();

                    blacklist_.emplace(process_name, info);
                }
            }
        } catch (std::exception &exc) {
            LOG_ERROR(CONFIG, "Encountering error while loading blacklist file. Error message: {}", exc.what());
            return;
        }
    }
    
    bool panic_blacklist::should_be_blocked(const std::string &process_name, const std::string &thread_name,
        const std::string &category, const std::int32_t code) {
        auto range = blacklist_.equal_range(process_name);
        for (auto it = range.first; it != range.second; ++it) {
            if ((it->second.thread_name_ == thread_name) && (it->second.category_ == category) && (it->second.code_ == code)) {
                return true;
            }
        }

        return false;
    }
}