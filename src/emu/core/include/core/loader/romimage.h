#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace eka2l1 {
    namespace loader {
        // Unstable

        struct rom_vsec_info {
            uint32_t secure_id;
            uint32_t vendor_id;
            uint32_t cap1;
            uint32_t cap2;
        };

        struct rom_header {
            uint32_t uid1;
            uint32_t uid2;
            uint32_t uid3;
            uint32_t uid_checksum;
            uint32_t entry_point;
            uint32_t code_address;
            uint32_t data_address;
            int32_t code_size;
            int32_t text_size;
            int32_t data_size;
            int32_t bss_size;
            int32_t heap_minimum_size;
            int32_t heap_maximum_size;
            int32_t stack_size;
            uint32_t dll_ref_table_address;
            int32_t export_dir_count;
            uint32_t export_dir_address;
            uint32_t code_checksum;
            uint32_t data_checksum;
            //rom_vsec_info sec_info;
            uint8_t major;
            uint8_t minor;
            uint16_t build;
            uint32_t flags_raw;
            uint32_t priority;
            uint32_t data_bss_linear_base_address;
            uint32_t next_extension_linear_address;
            uint32_t harware_variant; //I have no idea what this is exactly
            //uint32_t total_size;
            //uint32_t module;
            //uint32_t exception_des;
        };

        struct romimg {
            rom_header header;
            std::vector<uint32_t> exports;
        };

        std::optional<romimg> parse_romimg(const std::string& path);
    }
}
