/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <loader/gdr.h>
#include <common/buffer.h>
#include <common/log.h>

int main(int argc, char **argv) {
    eka2l1::log::setup_log(nullptr);

    if (argc <= 1) {
        LOG_ERROR("No file provided!");
        LOG_INFO("Usage: gdrdump [filename].");

        return -1;
    }

    const char *target_gdr = argv[1];

    eka2l1::common::ro_std_file_stream stream(target_gdr, true);
    eka2l1::loader::gdr::file_store store;

    if (!eka2l1::loader::gdr::parse_store(reinterpret_cast<eka2l1::common::ro_stream*>(&stream), store)) {
        LOG_ERROR("Error while parsing GDR file!");
        return -2;
    }

    return 0;
}
