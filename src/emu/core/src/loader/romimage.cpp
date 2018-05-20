#include <cstdio>
#include <common/log.h>
#include <core_mem.h>
#include <loader/romimage.h>
#include <hle/libmanager.h>
#include <disasm/disasm.h>
#include <vfs.h>

namespace eka2l1 {
    namespace loader {
        // Unstable
        std::optional<romimg> parse_romimg(symfile& file) {
            romimg img;
            file->read_file(&img, 1, sizeof(rom_image_header));
            file->seek(img.header.export_dir_address, file_seek_mode::beg);

            img.exports.resize(img.header.export_dir_count);

            for (auto& exp: img.exports) {
                file->read_file(&exp, 1, 4);
            }

            return img;
        }

        // This results the file also being modified
        bool stub_export(memory* mem, disasm* asmdis, address exp) {
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
        bool stub_romimg(romimg& img, std::string name, memory* mem, disasm* asmdis) {
            for (uint32_t i = 0; i < std::min(img.exports.size(), lib_sids.size()); i++) {
                stub_export(mem, asmdis, img.exports[i]);
            }
            
            return true;
        }
    }
}
