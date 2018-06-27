#pragma once

#include <cstdint>
#include <string>

#include <atomic>

namespace eka2l1 {
    class io_system;

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

        bool install_rpkg(io_system *io, const std::string &path, std::atomic<int> &res);
    }
}