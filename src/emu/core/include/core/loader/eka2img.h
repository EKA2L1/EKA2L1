#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <hle/libmanager.h>

// A lightweight loader based on elf2e32

namespace eka2l1 {
    namespace loader {
        enum class eka2_cpu : uint16_t {
            x86 = 0x1000,
            armv4 = 0x2000,
            armv5 = 0x2001,
            armv6 = 0x2002,
            mstar_core = 0x4000
        };

        enum class eka2_flags : uint32_t {
            exe = 0,
            dll = 1
        };

        enum class eka2_img_type : uint32_t {
            exe = 0x1000007a,
            dll = 0x10000079
        };

        // Copy from E32Explorer
        struct eka2img_import_block {
            //This is only for EKA1 targeted import block from PE
            uint32_t dll_name_offset; //relative to the import section
            int32_t number_of_imports; // number of imports from this dll
            std::vector<uint32_t> ordinals; // TUint32 iImport[iNumberOfImports];
            std::string dll_name;
        };

        struct eka2img_vsec_info {
            uint32_t secure_id;
            uint32_t vendor_id;
            uint32_t cap1;
            uint32_t cap2;
        };

        struct eka2img_header_extended {
            eka2img_vsec_info info;
            uint32_t exception_des;
            uint32_t spare2;
            uint16_t export_desc_size; // size of bitmap section
            uint8_t export_desc_type; // type of description of holes in export table
            uint8_t export_desc;
        };

        struct eka2_import_section {
            int32_t size; // size of this section
            std::vector<eka2img_import_block> imports; // E32ImportBlock[iDllRefTableCount];
        };

        // For my precious baby to yml
        struct eka2img_export_directory {
            std::vector<uint32_t> syms;
        };

        struct eka2img_iat {
            std::vector<uint32_t> its;
        };

        struct eka2_reloc_entry {
            uint32_t base;
            uint32_t size;

            std::vector<uint16_t> rels_info;
        };

		#define ELF32_R_SYM(i) ((i) >> 8)
		#define ELF32_R_TYPE(i) ((unsigned char)(i))
		#define ELF32_R_INFO(s, t) (((s) << 8) + (unsigned char)(t))

        struct eka2_reloc_section {
            uint32_t size;
            uint32_t num_relocs;

            std::vector<eka2_reloc_entry> entries;
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
            eka2img_header_extended header_extended;
            eka2img_iat iat;
            eka2img_export_directory ed;

            std::vector<char> data;
            uint32_t uncompressed_size;
            eka2_import_section import_section;

            eka2_reloc_section code_reloc_section;
            eka2_reloc_section data_reloc_section;

            uint32_t rt_code_addr;
            uint32_t rt_data_addr;

            bool has_extended_header = false;
        };

        std::optional<eka2img> parse_eka2img(const std::string &path, bool read_reloc = true);
        bool load_eka2img(eka2img &img, hle::lib_manager& mngr);
    }
}
