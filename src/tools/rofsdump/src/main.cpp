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

#include <loader/rofs.h>

#include <common/buffer.h>
#include <common/cvt.h>
#include <common/log.h>

int main(int argc, char **argv) {
    eka2l1::log::setup_log(nullptr);

    if (argc <= 1) {
        LOG_ERROR(eka2l1::SYSTEM, "No file provided!");
        LOG_INFO(eka2l1::SYSTEM, "Usage: rofsdump [filename].");

        return -1;
    }

    const char *target_rofs = argv[1];

    eka2l1::common::ro_std_file_stream stream(target_rofs, true);
    bool result = eka2l1::loader::dump_rofs_system(stream, "", nullptr, nullptr);

    return 0;
}
