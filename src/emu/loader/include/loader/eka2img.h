#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace eka2l1 {
    namespace loader {
        enum class eka2_cpu: uint16_t {
            x86 = 0x1000,
            armv4 = 0x2000,
            armv5=0x2001,
            armv6=0x2002,
            mstar_core = 0x4000
        };

        enum class eka2_flags: uint32_t {
            exe = 0,
            dll = 1
        };

        enum class eka2_img_type: uint32_t {
            exe = 0x1000007a,
            dll = 0x10000079
        };

        struct eka2img_header {
            eka2_img_type uid1;
            uint32_t uid2;
            uint32_t uid3;
            uint32_t check;
            uint32_t sig;

            //eka2_cpu cpu;

            uint32_t header_crc;

            uint16_t major;
            uint16_t minor;

            uint32_t compression_type;

            uint8_t petran_major;
            uint8_t petran_minor;
            uint16_t petran_build;

            uint32_t msb;
            uint32_t lsb;

            eka2_flags flags;
            uint32_t code_size;
            uint32_t data_size;

            uint32_t heap_size_min;
            uint32_t heap_size_max;

            uint32_t stack_size;
            uint32_t bss_size;
            uint32_t entry_point;

            uint32_t code_base;
            uint32_t data_base;
            uint32_t dll_ref_table_count;

            uint32_t export_dir_offset;
            uint32_t export_dir_count;

            uint32_t text_size;
            uint32_t code_offset;
            uint32_t data_offset;

            uint32_t import_offset;
            uint32_t code_reloc_offset;
            uint32_t data_reloc_offset;

            uint16_t priority;
            eka2_cpu cpu;
        };

        struct eka2img {
            eka2img_header header;
            std::vector<char> data;
            uint32_t uncompressed_size;
        };

        eka2img load_eka2img(const std::string& path);
    }
}
