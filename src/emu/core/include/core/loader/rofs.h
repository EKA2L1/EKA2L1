#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Extracting the one and annoying only Read Only File System!!

namespace eka2l1 {
    namespace loader {
        struct rofs_header {
            uint32_t magic;
            uint8_t header_size;
            uint8_t reserved;
            uint16_t rofs_format_ver;
            uint32_t dir_tree_off;
            uint32_t dir_tree_size;
            uint32_t dir_file_entries_off; // offset to start of file entries
            uint32_t dir_file_entries_size; // size in bytes of file entry block
            uint64_t time;
            uint8_t major;
            uint8_t minor;
            uint32_t img_size;
            uint32_t checksum;
            uint32_t max_img_size;
        };

        struct rofs_extension_header : public rofs_header {};

        struct rofs_entry {
            uint16_t iStructSize; // Total size of entry, header + name + any padding
            uint32_t uids; // A copy of all the UID info
            uint8_t name_off; // offset of iName from start of entry
            uint8_t attribute; // standard file attributes
            uint32_t file_size; // real size of file in bytes (may be different from size in image)
                // for subdirectories this is the total size of the directory
                // block entry excluding padding
            uint32_t file_addr; // address in image of file start
            uint8_t attrib_extra; // extra ROFS attributes (these are inverted so 0 = enabled)
            uint8_t name_len; // length of iName
            std::string name;
        };

        struct rofs_dir {
            uint16_t struct_size; // Total size of this directory block including padding
            uint8_t padding;
            uint8_t first_entry_off; // offset to first entry
            uint32_t file_block_addr; // address of associated file block
            uint32_t file_block_size; // size of associated file block
            rofs_entry sub_dir;
        };

        struct rofs_file {
            rofs_header header;
            std::vector<rofs_entry> entries;
            std::vector<rofs_dir> dirs;
        };
    }
}
