#include <algorithm>
#include <cstdio>
#include <common/log.h>
#include <core_mem.h>
#include <loader/romimage.h>
#include <hle/libmanager.h>
#include <disasm/disasm.h>
#include <common/algorithm.h>
#include <vfs.h>

#include <set>

namespace eka2l1 {
    namespace loader {
		uint32_t rom_to_offset_n(address romstart, address off) {
			return off - romstart;
		}

        // Unstable
        std::optional<romimg> parse_romimg(symfile& file, memory_system* mem) {
            romimg img;
            file->read_file(&img, 1, sizeof(rom_image_header));

			ptr<uint32_t> export_off(img.header.export_dir_address);

			for (uint32_t i = 0; i < img.header.export_dir_count; i++) {
				auto export_addr = *export_off.get(mem);
				img.exports.push_back(export_addr);

				export_off += 4;
            }
			
            return img;
        }

        // This results the file also being modified
        bool stub_export(memory_system* mem, disasm* asmdis, address exp) {
            subroutine sub = asmdis->get_subroutine(ptr<uint8_t>(exp));

            // Fill them with nop
            int ad = 0;

            // Stub them with zero
            for (auto& inst: sub.insts) {
                if (inst == arm_inst_type::thumb) {
                    *(ptr<uint16_t>(exp + ad).get(mem)) = 0x46c0;
                    ad += 2;
                } else if (inst == arm_inst_type::arm) {
                    *(ptr<uint32_t>(exp+ad).get(mem)) = 0x00000000;
                    ad += 4;
                } else {
                    ad += 1;
                }
            }

            return true;
        }

        // Stub the export with NOP
        bool stub_romimg(romimg& img, memory_system* mem, disasm* asmdis) {
            for (uint32_t i = 0; i < img.exports.size(); i++) {
				stub_export(mem, asmdis, img.exports[i]);
            }
            
            return true;
        }
    }
}
