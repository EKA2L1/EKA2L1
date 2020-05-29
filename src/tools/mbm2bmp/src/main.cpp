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

#include <loader/mbm.h>

#include <common/buffer.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <common/virtualmem.h>

#include <iostream>
#include <sstream>

int main(int argc, char **argv) {
    eka2l1::log::setup_log(nullptr);

    if (argc <= 1) {
        LOG_ERROR("No file provided!");
        LOG_INFO("Usage: mbm2bmp [filename].");

        return -1;
    }

    const char *target_mbm = argv[1];

    // Map the file into memory as read-only
    std::uint8_t *mbm_ptr = reinterpret_cast<std::uint8_t *>(eka2l1::common::map_file(target_mbm));

    if (!mbm_ptr) {
        LOG_ERROR("Try to map MBM file to memory but failed, exiting...");
        return -2;
    }

    eka2l1::common::ro_buf_stream stream(mbm_ptr, eka2l1::common::file_size(target_mbm));
    eka2l1::loader::mbm_file mbmf(reinterpret_cast<eka2l1::common::ro_stream *>(&stream));

    if (!mbmf.do_read_headers()) {
        LOG_ERROR("Reading MBM header failed! At least one magic value doesn't match!");
        return -3;
    }

    for (std::size_t i = 0; i < mbmf.trailer.count; i++) {
        std::string bmp_name = eka2l1::replace_extension(eka2l1::filename(target_mbm), "");
        bmp_name += "_" + eka2l1::common::to_string(i) + ".bmp";
        if (!mbmf.save_bitmap_to_file(i, bmp_name.c_str())) {
            LOG_ERROR("Can't save bitmap number {}!", i);
        }
    }

    return 0;
}
