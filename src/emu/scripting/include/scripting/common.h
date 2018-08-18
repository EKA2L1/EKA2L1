#pragma once

#include <common/bytepair.h>
#include <vector>

namespace eka2l1::scripting {
    struct ibytepair_stream_wrapper {
        common::ibytepair_stream bytepair_stream;

    public:
        ibytepair_stream_wrapper(const std::string &path);

        std::vector<uint32_t> get_all_page_offsets();
        void read_page_table();

        std::vector<char> read_page(uint16_t index);
        std::vector<char> read_pages();
    };
}