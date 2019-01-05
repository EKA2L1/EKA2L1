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

#pragma once

#include <functional>
#include <string>
#include <vector>

#include <epoc/loader/sis_fields.h>

namespace eka2l1 {
    class io_system;

    namespace manager {
        class package_manager;
    }

    namespace loader {
        // An interpreter that runs SIS install script
        class ss_interpreter {
            sis_install_block install_block;
            sis_data install_data;
            drive_number install_drive;

            std::shared_ptr<std::istream> data_stream;
            std::function<bool(std::vector<uint8_t>)> show_text_func;

            io_system *io;
            manager::package_manager *mngr;

            bool appprop(sis_uid uid, sis_property prop);
            bool package(sis_uid uid);

        public:
            std::vector<uint8_t> get_small_file_buf(uint32_t data_idx, uint16_t crr_blck_idx);
            void extract_file(const std::string &path, const uint32_t idx, uint16_t crr_blck_idx);

            explicit ss_interpreter();
            ss_interpreter(std::shared_ptr<std::istream> stream,
                io_system *io,
                manager::package_manager *pkgmngr,
                sis_install_block inst_blck,
                sis_data inst_data,
                drive_number install_drv);

            bool interpret(sis_install_block install_block, uint16_t crr_blck_idx = 0);
            bool interpret() { return interpret(install_block); }
        };
    }
}
