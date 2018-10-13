/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <common/algorithm.h>
#include <common/bytepair.h>
#include <common/log.h>

#include <cstdint>
#include <functional>
#include <stack>

namespace eka2l1 {
    namespace common {
        // Given a chunk of data compress by byte-pair compression as specified by Nokia,
        // decompress the chunk.
        // The game which I'm testing that use this type of compression (and even its pak file),
        // is Super Miners by ID
        int nokia_bytepair_decompress(void *destination, unsigned int dest_size, void *buffer, unsigned int buf_size) {
            uint8_t *data8 = reinterpret_cast<uint8_t *>(buffer);
            uint32_t lookup_table[0x200];

            uint8_t *lookup_table_first = reinterpret_cast<uint8_t *>(lookup_table);
            uint8_t *lookup_table_second = lookup_table_first + 0x100;

            uint32_t marker = ~0u;
            uint8_t p1, p2;

            // Fill the table
            uint32_t b = 0x03020100;
            uint32_t step = 0x04040404;

            std::stack<uint8_t> sec_stack;

            uint8_t *buf_end = reinterpret_cast<uint8_t *>(buffer) + buf_size;
            uint8_t *dest_end = reinterpret_cast<uint8_t *>(destination) + dest_size;

            uint8_t *dest = reinterpret_cast<uint8_t *>(destination);

            uint32_t *lut = (uint32_t *)(lookup_table);

            do {
                *lut++ = b;
                b += step;
            } while (b > step);

            uint8_t total_pair = *data8++;

            if (total_pair) {
                marker = *data8++;
                lookup_table_first[marker] = (uint8_t)(~marker);

                if (total_pair < 32) {
                    uint8_t *pair_end = data8 + 3 * total_pair;

                    do {
                        b = *data8++;
                        p1 = *data8++;
                        p2 = *data8++;

                        lookup_table_first[b] = p1;
                        lookup_table_second[b] = p2;
                    } while (data8 < pair_end);

                } else {
                    uint8_t *mask_st = data8;
                    data8 += 32;

                    for (b = 0; b < 0x100; b++) {
                        uint8_t mask = mask_st[b >> 3];

                        if (mask & (1 << (b & 7))) {
                            // Read the pair
                            p1 = *data8++;
                            p2 = *data8++;

                            lookup_table_first[b] = p1;
                            lookup_table_second[b] = p2;

                            --total_pair;
                        }
                    }

                    if (total_pair) {
                        return 0;
                    }
                }
            }

            b = *data8++;
            p1 = lookup_table_first[b];

            // I'm so depressed that i have to do this
            if (p1 != b)
                goto not_single;

        process_replace:
            if (data8 >= buf_end) {
                goto done_data8;
            }

            b = *data8++;
            *dest++ = p1;

            if (dest >= dest_end) {
                goto done_dest;
            }

            p1 = lookup_table_first[b];

            if (p1 == b)
                goto process_replace;

        not_single:
            if (b == marker) {
                goto do_marker;
            }

        do_pair:
            p2 = lookup_table_second[b];
            b = p1;
            p1 = lookup_table_first[b];
            sec_stack.push(p2);

        recurse:
            if (b != p1) {
                goto do_pair;
            }

            if (sec_stack.empty()) {
                goto process_replace;
            }

            b = sec_stack.top();
            sec_stack.pop();

            *dest++ = p1;
            p1 = lookup_table_first[b];

            goto recurse;

        do_marker:
            p1 = *data8++;
            goto process_replace;

        done_data8:
            *dest++ = p1;
            return static_cast<int>(dest - static_cast<uint8_t *>(destination));
            
        done_dest:
            return static_cast<int>(dest - static_cast<uint8_t *>(destination));

            return 1;
        }

        ibytepair_stream::ibytepair_stream(std::shared_ptr<std::istream> stream) {
            compress_stream = stream;
        }

        ibytepair_stream::ibytepair_stream(std::string path, uint32_t start) {
            compress_stream = std::make_shared<std::ifstream>(path, std::ios::binary);
            compress_stream->seekg(start);
        }

        ibytepair_stream::index_table ibytepair_stream::table() const {
            return idx_tab;
        }

        void ibytepair_stream::seek_fwd(size_t size) {
            compress_stream->seekg(size, std::ios::cur);
        }

        // Read the table entry
        void ibytepair_stream::read_table() {
            compress_stream->read(reinterpret_cast<char *>(&idx_tab.header), 10);
            idx_tab.page_size.resize(idx_tab.header.number_of_pages);

            compress_stream->read(reinterpret_cast<char *>(idx_tab.page_size.data()),
                idx_tab.page_size.size() * sizeof(uint16_t));
        }

        uint32_t ibytepair_stream::read_page(char *dest, uint32_t page, size_t size) {
            size_t len = common::min<size_t>(size, 4096);
            auto crr_pos = compress_stream->tellg();
            std::vector<char> buf;

            buf.resize(idx_tab.page_size[page]);

            compress_stream->read(buf.data(), buf.size());

            if (buf.data() == nullptr) {
                LOG_ERROR("Buffer is null! Size: {}", idx_tab.page_size[page]);
                return 0;
            }

            auto omitted = nokia_bytepair_decompress(dest, static_cast<int>(len), buf.data(), idx_tab.page_size[page]);

            return omitted;
        }

        uint32_t ibytepair_stream::read_pages(char *dest, size_t size) {
            uint32_t decompressed_size = 0;
            read_table();

            uint64_t tsize = 0;

            for (uint32_t i = 0; i < idx_tab.page_size.size(); i++) {
                tsize += idx_tab.page_size[i];
            }

            LOG_INFO("Total size: {}, needed size: {}", tsize, size);

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

            size_t bytes = initial_off + 10 + res.size() * sizeof(uint16_t);

            for (auto i = 0; i < res.size(); ++i) {
                res[i] = static_cast<uint32_t>(bytes);
                bytes += idx_tab.page_size[i];
            }

            res[res.size()] = static_cast<uint32_t>(bytes);

            return res;
        }
    }
}