/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include <j2me/common.h>

namespace eka2l1 {
    class system;
}

namespace eka2l1::j2me {
    struct app_entry;
    class app_list;

    void launch_through_kmidrun(system *sys, const app_entry &entry);
    install_error install_for_kmidrun(system *sys, app_list *applist, const std::string &path, app_entry &entry_ino);
    void uninstall_for_kmidrun(system *sys, app_list *applist, const app_entry &entry);
}