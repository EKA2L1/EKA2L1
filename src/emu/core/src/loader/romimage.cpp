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
#include <common/algorithm.h>
#include <common/log.h>
#include <core_mem.h>
#include <cstdio>
#include <disasm/disasm.h>
#include <hle/libmanager.h>
#include <loader/romimage.h>
#include <vfs.h>

#include <set>

namespace eka2l1 {
    namespace loader {
        uint32_t rom_to_offset_n(address romstart, address off) {
            return off - romstart;
        }

        // Unstable
        std::optional<romimg> parse_romimg(symfile &file, memory_system *mem) {
            romimg img;
            file->read_file(&img, 1, sizeof(rom_image_header));

            ptr<uint32_t> export_off(img.header.export_dir_address);

            for (uint32_t i = 0; i < img.header.export_dir_count; i++) {
                auto export_addr = *export_off.get(mem);

                //if (img.exports.size())
                    img.exports.push_back(export_addr);

                export_off += 4;
            }

            return img;
        }

        // This results the file also being modified
        bool stub_export(memory_system *mem, disasm *asmdis, address exp) {
            subroutine sub = asmdis->get_subroutine(ptr<uint8_t>(exp));

            // Fill them with nop
            int ad = 0;

            // Stub them with zero
            for (auto &inst : sub.insts) {
                if (inst == arm_inst_type::thumb) {
                    *(ptr<uint16_t>(exp + ad).get(mem)) = 0x46c0;
                    ad += 2;
                } else if (inst == arm_inst_type::arm) {
                    *(ptr<uint32_t>(exp + ad).get(mem)) = 0x00000000;
                    ad += 4;
                } else {
                    ad += 1;
                }
            }

            return true;
        }

        // Stub the export with NOP
        bool stub_romimg(romimg &img, memory_system *mem, disasm *asmdis) {
            for (uint32_t i = 0; i < img.exports.size(); i++) {
                stub_export(mem, asmdis, img.exports[i]);
            }

            return true;
        }
    }
}

