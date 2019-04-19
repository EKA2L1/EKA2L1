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

#include <algorithm>
#include <debugger/debugger.h>
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace eka2l1 {
    void debugger_base::set_breakpoint(const std::uint32_t bkpt_addr, const bool hit,
        const bool new_one) {
        const std::lock_guard<std::mutex> guard(lock);

        auto bkpt = std::find_if(breakpoints.begin(), breakpoints.end(), [bkpt_addr](debug_breakpoint &b) {
            return b.addr == bkpt_addr;
        });

        if (bkpt == breakpoints.end() && new_one) {
            breakpoints.push_back(debug_breakpoint{ bkpt_addr, hit });

            std::stable_sort(breakpoints.begin(), breakpoints.end(), [](const debug_breakpoint &lhs, const debug_breakpoint &rhs) {
                return lhs.addr < rhs.addr;
            });
        }
    }

    void debugger_base::unset_breakpoint(const std::uint32_t bkpt_addr) {
        const std::lock_guard<std::mutex> guard(lock);

        auto bkpt = std::find_if(breakpoints.begin(), breakpoints.end(), [bkpt_addr](debug_breakpoint &b) {
            return b.addr == bkpt_addr;
        });

        if (bkpt != breakpoints.end()) {
            breakpoints.erase(bkpt);

            std::stable_sort(breakpoints.begin(), breakpoints.end(), [](const debug_breakpoint &lhs, const debug_breakpoint &rhs) {
                return lhs.addr < rhs.addr;
            });
        }
    }

    std::optional<debug_breakpoint> debugger_base::get_nearest_breakpoint(const std::uint32_t bkpt) {
        if (breakpoints.size() == 0) {
            return std::optional<debug_breakpoint>{};
        }

        for (std::size_t i = 0; i < breakpoints.size(); i++) {
            if (breakpoints[i].addr > bkpt) {
                if (i == 0) {
                    break;
                }

                return breakpoints[i - 1];
            }
        }

        return breakpoints[breakpoints.size() - 1];
    }

    void skin_state::serialize() {
        YAML::Emitter emitter;
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "bkg_alpha" << YAML::Value << bkg_transparency;
        emitter << YAML::Key << "bkg_path" << YAML::Value << bkg_path;
        emitter << YAML::Key << "font" << YAML::Value << font_path;

        std::ofstream file("skin.yml");
        file << emitter.c_str();
    }

    void skin_state::deserialize() {
        bkg_path = "";
        font_path = "";
        bkg_transparency = 129;

        YAML::Node node;

        try {
            node = YAML::LoadFile("skin.yml");
        } catch (YAML::BadFile &exc) {
            return;
        }

        try {
            bkg_path = node["bkg_path"].as<std::string>();
        } catch (...) {
            bkg_path = "";
        }

        try {
            bkg_transparency = node["bkg_alpha"].as<int>();
        } catch (...) {
            bkg_transparency = 129;
        }
        
        try {
            font_path = node["font"].as<std::string>();
        } catch (...) {
            font_path = "";
        }
    }
}
