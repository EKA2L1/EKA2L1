#include <common/bytepair.h>
#include <common/algorithm.h>

#include <cstdint>
#include <functional>
#include <stack>


namespace eka2l1 {
    namespace common {

        // Given a chunk of data compress by byte-pair compression as specified by Nokia,
        // decompress the chunk.
        // The game which I'm testing that use this type of compression (and even its pak file),
        // is Super Miners by ID
        int nokia_bytepair_decompress(void* destination, unsigned int dest_size, void* buffer, unsigned int buf_size) {
            uint8_t* data8 = reinterpret_cast<uint8_t*>(buffer);
            uint32_t lookup_table[0x100];

            uint8_t* lookup_table_first = reinterpret_cast<uint8_t*>(lookup_table);
            uint8_t* lookup_table_second = lookup_table_first + 0x100;

            // Fill the table
            uint32_t b = 0x03020100;
            uint32_t step = 0x04040404;

            std::stack<uint8_t> sec_stack;

            uint8_t* buf_end = reinterpret_cast<uint8_t*>(buffer) + buf_size;
            uint8_t* dest_end = reinterpret_cast<uint8_t*>(destination) +dest_size;

            uint8_t* dest = reinterpret_cast<uint8_t*>(destination);

            uint32_t* lut = (uint32_t*)(lookup_table);

            do
            {
                *lut++ = b;
                b+= step;
            } while (b > step);

            uint8_t total_pair = *data8++;

            if (total_pair) {
                uint8_t marker = *data8++;
                lookup_table_first[marker] = ~marker;

                if (total_pair < 32) {
                    uint8_t* pair_end = data8 + 3 * total_pair;

                    do {
                        auto b = *data8++;
                        auto p1 = *data8++;
                        auto p2 = *data8++;

                        lookup_table_first[b] = p1;
                        lookup_table_second[b] = p2;
                    } while (data8 < pair_end);

                } else {
                    uint8_t* mask_st = data8;
                    data8 += 32;

                    for (uint32_t b = 0; b < 0x100; b++) {
                        uint8_t mask = mask_st[b >> 3];

                        if (mask & (1<< (b & 7))) {
                            // Read the pair
                            lookup_table_first[b] = *data8++;
                            lookup_table_second[b] = *data8++;
                            --total_pair;
                        }
                    }
                }

                if (total_pair) {
                    return 0;
                }

                b = *data8++;
                uint8_t p1 = lookup_table_first[b];
                uint8_t p2;

                // I translate Nokia's label block to C++ 14 lambda

                auto done_data8 = [&]() {
                    *dest = p1;
                    return 1;
                };

                auto done_dest = [&]() {
                    return 1;
                };

                auto do_marker = [&]() {
                   p1 = *data8++;
                };

                std::function<void()> process_replace;
                std::function<void()> not_single;

                process_replace = [&]() {
                    if (data8 >= buf_end) {
                        done_data8();
                    }

                    if (dest >= dest_end) {
                        done_dest();
                    }

                    b = *data8++;
                    *dest++ = p1;

                    p1 = lookup_table_first[b];

                    if (p1 == b) {
                        process_replace();
                    } else {
                        not_single();
                    }
                };

                std::function<void()> do_pair;
                std::function<void()> recurse;

                recurse = [&]() {
                    if (b != p1) {
                        do_pair();
                    }

                    if (sec_stack.empty()) {
                        process_replace();
                    }

                    b = sec_stack.top();
                    sec_stack.pop();
                    *dest++ = p1;
                    p1 = lookup_table_first[b];

                    recurse();
                };

                do_pair = [&]() {
                    p2 = lookup_table_second[b];
                    b = p1;
                    p1 = lookup_table_first[b];
                    sec_stack.push(p2);

                    recurse();
                };

                not_single = [&]() {
                    if (b == marker) {
                        do_marker();
                    } else {
                        do_pair();
                    }
                };

                // The pair is not single
                if (p1 != b) {
                    not_single();
                } else {
                    process_replace();
                }
            }

            return 1;
        }

        ibytepair_stream::ibytepair_stream(std::shared_ptr<std::istream> stream)
            : compress_stream(std::move(stream)) {
        }

        ibytepair_stream::index_table ibytepair_stream::table() const {
            return idx_tab;
        }

        void ibytepair_stream::seek_fwd(size_t size) {
            compress_stream->seekg(size, std::ios::cur);
        }

        // Read the table entry
        void ibytepair_stream::read_table() {
            compress_stream->read(reinterpret_cast<char*>(&idx_tab.header), 10);
            idx_tab.page_size.resize(idx_tab.header.number_of_pages);

            compress_stream->read(reinterpret_cast<char*>(idx_tab.page_size.data()),
                                  idx_tab.page_size.size() * sizeof(uint16_t));
        }

        uint32_t ibytepair_stream::read_page(char* dest, uint32_t page, size_t size) {
            uint32_t len = common::min<uint32_t>(size, idx_tab.page_size[page]);
            auto crr_pos = compress_stream->tellg();
            std::vector<char> buf(len);
            compress_stream->read(buf.data(), len);
            auto omitted = nokia_bytepair_decompress(dest, len, buf.data(), idx_tab.page_size[page]);
            compress_stream->seekg(crr_pos, std::ios::beg);

            return omitted;
        }

        uint32_t ibytepair_stream::read_pages(char* dest, size_t size) {
            uint32_t decompressed_size = 0;
            read_table();

            for (auto i = 0; i < idx_tab.header.number_of_pages; i++) {
                uint32_t dcs_page = read_page(dest, i, size);

                decompressed_size += dcs_page;
                dest += dcs_page;
                size -= dcs_page;
            }

            return decompressed_size;
        }

        std::vector<uint32_t> ibytepair_stream::page_offsets(uint32_t initial_off) {
            read_table();
            std::vector<uint32_t> res;

            res.resize(idx_tab.header.number_of_pages + 1);

            uint32_t bytes = initial_off + 10 + res.size() * sizeof(uint16_t);
            for (auto i = 0; i < res.size(); ++i) {
                res[i] = bytes;
                bytes += idx_tab.page_size[i];
            }

            res[res.size()] = bytes;

            return res;
        }
    }
}
