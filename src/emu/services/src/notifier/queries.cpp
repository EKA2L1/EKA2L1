/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <services/notifier/queries.h>
#include <services/ui/notenof.h>

namespace eka2l1::epoc::notifier  {
    void add_builtin_plugins(kernel_system *kern, std::vector<plugin_instance> &plugins) {
#define ADD_PLUGIN(name) plugins.push_back(std::make_unique<name>(kern))
        ADD_PLUGIN(note_display_plugin);
#undef ADD_PLUGIN

        std::sort(plugins.begin(), plugins.end(), [](const plugin_instance &lhs, const plugin_instance &rhs) {
            return lhs->unique_id() < rhs->unique_id();
        });
    }
};