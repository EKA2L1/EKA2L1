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

#include <epoc/services/akn/skin/skn.h>
#include <epoc/vfs.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/types.h>

int main(int argc, char **argv) {
    eka2l1::log::setup_log(nullptr);

    if (argc <= 1) {
        LOG_ERROR("No file provided!");
        LOG_INFO("Usage: skninfo [filename].");
        
        return -1;
    }

    eka2l1::symfile f = eka2l1::physical_file_proxy(argv[1], READ_MODE | BIN_MODE);
    eka2l1::ro_file_stream f_stream(f.get());

    eka2l1::epoc::skn_file sknf(reinterpret_cast<eka2l1::common::ro_stream*>(&f_stream));

    // Print out skin name
    LOG_INFO("Skin name: {}, language: {}", eka2l1::common::ucs2_to_utf8(sknf.skin_name_.name),
        num_to_lang(sknf.skin_name_.lang));

    LOG_INFO("Compiler version: {}.{}", sknf.info_.version >> 16, sknf.info_.version & 0xFFFF);
    LOG_INFO("Platform: {}.{}", sknf.info_.plat >> 8, sknf.info_.plat & 0xFF);
    LOG_INFO("Author: {}", sknf.info_.author.empty() ? "Unknown" : eka2l1::common::ucs2_to_utf8(sknf.info_.author));

    if (!sknf.info_.copyright.empty()) {
        LOG_INFO("{}", eka2l1::common::ucs2_to_utf8(sknf.info_.copyright));
    }

    std::string listed_name = "Associated filenames: \n";

    for (auto &filename_: sknf.filenames_) {
        listed_name += fmt::format("\t- {} (ID: {})\n", eka2l1::common::ucs2_to_utf8(filename_.second),
            filename_.first);
    }

    LOG_INFO("{}", listed_name);

    return 0;
}
