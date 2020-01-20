#pragma once

#include <common/types.h>

#include <atomic>
#include <cstdint>
#include <string>

namespace eka2l1 {
    class io_system;

    namespace manager {
        class device_manager;
    }

    namespace loader {
        struct rpkg_header {
            uint32_t magic[4];
            uint8_t major_rom;
            uint8_t minor_rom;
            uint16_t build_rom;
            uint32_t count;
        };

        struct rpkg_entry {
            uint64_t attrib;
            uint64_t time;
            uint64_t path_len;

            std::u16string path;

            uint64_t data_size;
        };

        bool install_rpkg(manager::device_manager *dvc, const std::string &path,
            const std::string &devices_rom_path, std::string &firmware_code, std::atomic<int> &res);
    }
}