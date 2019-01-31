/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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
#include <common/buffer.h>
#include <common/chunkyseri.h>
#include <common/dictcomp.h>
#include <common/log.h>
#include <common/unicode.h>

#include <epoc/loader/rsc.h>
#include <epoc/vfs.h>

#include <stack>

namespace eka2l1::loader {
    /*
       The header format should be readed like this:
       - First 12 bytes should contain 3 UID type
       - The first UID will divide the RSC file into 3 kind:
          * 0x101f4a6b: This will potentially contains compressed unicode data, which is compressed using run-len
          * 0x101f5010: This will potentially contains compressed unicode data + will be compressed using dictionary algroithm
       - If it doesn't fall into 2 of these types, discard all of these read result.
       - Read the first 2 bytes, if it's equals to 4 than it is fallen into a format called "Calypso" and dictionary compressed.

        Calypso:
            - 0 size = 2: Magic (4)
            - 2 size = 2: Number of resources in this file
            - 4 size = 1: unk1
            - 5 size = 1: unk2
            - 6 size = 2: Offset of resource index
            - 8 size = 2: Size of largest resource when decompress

        Normal:
            - 0 size = 12: 3 UID
            - 12 size = 4: UID?
            - 16 Size = 1: Flags 
              + 0x80: third uid = offset
              + 0x40: RSS signature is generated for first resource.
              + 0x20: First resource is a bit array containings information telling us if a specific resource
                      has Unicode compressed in its data or not.
            - 17 Size = 2: size of largest resource when decompress
            - 19: 
               * Compressed: size = 2: resource data offset
               * Potientially contains Unicode compressed: Unicode bit array
            
            - 21:
              If the first resource not containing the bits array that indicates the appearance of Unicode compressed data in other resource,
                the next *number_of_resource* butes will contain a bit array.
              + Last two bytes of the file will tell us the total bit in resource data section (they resides in index table)
              + Use that to calculate the beginning of index section. Each entry of index section will be 2 bytes, so
                calculate the number of resources in the file should be easy, by (subtracting EOF offset and the resource index begin offset) div 2
              + 8 unicode bit for 8 resource will be packed into one byte. Bit 1 = Contains unicode, Bit 0 = Not contains unicode.
              + To see if the resource section actually contains Unicode, with the resource index given (0, 1 for e.g), get the byte that the 
                Unicode bit for this resource index is belonged in. Getting this should be easy by dividing the resource index by 8

                (Note that we are counting from the lowest bytes)
                11101010         10110001        (Base resource is 0, base byte is 0)
                     ^              ^
                Resource 2      Resource 12
                Byte 0          Byte 1

              + After getting the byte, get the bit by creating a simple mask: shift 1 left by (resource index % 8) bits
              + 1 shl 2 = 0b00000100
                11101010 & 00000100 = 0 => Not contaning unicode
              + Sure these engineers who design this format are genius well ;)

      - Section layout: 
        Normal RSC file (Not calypso):
            * Dictionary data
            * Dictionary index. Last 2 bytes of it seems to be the whole size of dict section
            * Resource data
            * Resource index. Last 2 bytes of it seems to be the whole size of resource data section
        
        Each entry of both index sections is 2 bytes, so it should give you something. They give information
        about 
            * If dictionary compressed: the offset of the dict/resource entry from the dictionary/resource data
            * If the RSC not compressed, each index will points to offset of resource entry based on offset 0

        Calypso file:
            * Dictionary index
            * Dictionary data
            * Resource index
            * Resource data
    */

    bool rsc_file::is_resource_contains_unicode(int res_id, bool first_rsc_is_gen) {
        if (first_rsc_is_gen) {
            // First resource is that generated bit array
            --res_id;

            if (res_id < 0) {
                return false;
            }
        }

        if (unicode_flag_array.empty()) {
            return false;
        }

        // See up above for information about this
        return unicode_flag_array[res_id / 8] & (1 << (res_id % 8));
    }

    int rsc_file::decompress(std::uint8_t *buffer, int max, int res_index) {
        if (!(flags & dictionary_compressed)) {
            // We just read as normal
            int read_size_bytes = 0;
            
            if (res_index == resource_offsets.size() - 1) {
                read_size_bytes = 
                    static_cast<int>(res_data.size() + resource_offsets[0] - resource_offsets.back());
            } else {
                read_size_bytes = resource_offsets[res_index + 1] - resource_offsets[res_index];
            }

            if (read_size_bytes <= 0 || read_size_bytes > max) {
                return -1;
            }

            std::memcpy(buffer, &res_data[resource_offsets[res_index] - res_offset], read_size_bytes);
            return read_size_bytes;
        }

        std::stack<common::dictcomp> streams;

        auto append_dictcomp_stream = [&](int resource_index) {
            std::uint16_t begin_bits = resource_offsets[resource_index - 1];
            std::uint16_t end_bits = resource_offsets[resource_index];

            // I don't use any cache though
            common::dictcomp comp_stream(&res_data[0], begin_bits, end_bits, num_of_bits_use_for_dict_token);
            streams.push(std::move(comp_stream));
        };

        append_dictcomp_stream(res_index);

        bool is_calypso = (flags & calypso);
        int total_bytes = 0;

        for (;;) {
            if (streams.empty()) {
                break;
            }

            common::dictcomp comp_stream = std::move(streams.top());
            streams.pop();

            for (;;) {
                const int index_of_dict_entry = comp_stream.index_of_current_directory_entry();
                if (index_of_dict_entry < 0) {
                    int result = comp_stream.read(buffer, max, is_calypso);

                    if (result == -1) {
                        return result;
                    }

                    total_bytes += result;
                    max -= result;
                } else {
                    append_dictcomp_stream(index_of_dict_entry);
                    break;
                }
            }
        }

        return total_bytes;
    }

    void rsc_file::read_header_and_resource_index(common::ro_buf_stream &buf) {
        std::uint32_t uid[3];
        buf.read(&uid, 12);

        std::memcpy(&uids, &uid[0], 12);

        switch (uid[0]) {
        case 0x101F4A6B: {
            flags |= potentially_have_compressed_unicode_data;

            buf.read(17, &size_of_largest_resource_when_uncompressed, 
                sizeof(size_of_largest_resource_when_uncompressed));

            break;
        }

        case 0x101F5010: {
            flags |= (potentially_have_compressed_unicode_data | dictionary_compressed);
            buf.read(17, &size_of_largest_resource_when_uncompressed, sizeof(size_of_largest_resource_when_uncompressed));

            break;
        }

        default: {
            break;
        }
        }

        // Check for calypso
        std::uint16_t calypso_magic = uid[0] >> 16;
        if (calypso_magic == 4) {
            flags |= (calypso | dictionary_compressed);

            buf.read(8, &size_of_largest_resource_when_uncompressed, sizeof(size_of_largest_resource_when_uncompressed));
        }

        if (flags & dictionary_compressed) {
            if (flags & calypso) {
                // We read it before
                num_res = uid[0] & 0xFFFF;
                buf.read(10, &num_of_bits_use_for_dict_token, sizeof(num_of_bits_use_for_dict_token));

                char unk1 = 0;

                buf.read(5, &unk1, 1);

                num_dir_entry = (1 << num_of_bits_use_for_dict_token) - unk1;

                buf.read(6, &res_index_offset, sizeof(res_index_offset));

                res_offset = res_index_offset + (num_res * 2);
                
                resource_offsets.resize(num_res);
                buf.read(res_index_offset, &resource_offsets[0], 2 * num_res);
                
                // "+2" because the first entry in the dictionary-index in this file format 
                //is the number of bits from the start of the dictionary data to the start 
                //of the first dictionary entry which is always zero, and thus unnecessary
                dict_index_offset = 4+7+2; 

                // Duplicate by 2 because each index has size of 2
                dict_offset = dict_index_offset + (num_dir_entry * 2);
                
                dict_offsets.resize(num_dir_entry);
                buf.read(dict_index_offset, &dict_offsets[0], 2 * num_dir_entry);
            } else {
                std::uint8_t file_flag = 0;
                buf.read(16, &file_flag, sizeof(file_flag));

                // Check the flag
                // 0x80: Third uid is an offset
                if (file_flag & 0x80) {
                    flags |= third_uid_offset;
                }

                if (file_flag & 0x40) {
                    flags |= generate_rss_sig_for_first_user_res;
                }

                if (file_flag & 0x20) {
                    flags |= first_res_generated_bit_array_of_res_contains_compressed_unicode;
                }

                // After the flag and some two byte
                res_offset = 19;
                std::uint16_t num_bits_of_res_data = 0;

                buf.read(buf.size() - 2, &num_bits_of_res_data, 2);
                res_index_offset = res_offset + (num_bits_of_res_data + 7) / 8;

                res_data.resize((num_bits_of_res_data + 7) / 8);
                buf.read(res_offset, &res_data[0], static_cast<std::uint32_t>(res_data.size()));

                // Each resource entry is two bytes.
                num_res = static_cast<std::uint16_t>((buf.size() - res_index_offset) / 2);
                
                resource_offsets.resize(num_res + 1);
                buf.read(res_index_offset, &resource_offsets[0], 2 * num_res);

                dict_offset = 21;

                if ((num_res > 0) && !(flags & first_res_generated_bit_array_of_res_contains_compressed_unicode)) {
                    int length_of_bit_array_in_bytes = (num_res + 7) / 8;
                    unicode_flag_array.resize(length_of_bit_array_in_bytes);

                    buf.read(21, &unicode_flag_array[0], length_of_bit_array_in_bytes);

                    dict_offset += length_of_bit_array_in_bytes;
                }

                std::uint16_t num_bits_of_dict_data = 0;
                buf.read(res_offset - 2, &num_bits_of_dict_data, 2);

                dict_index_offset = dict_offset + (num_bits_of_dict_data + 7) / 8;

                // Each dictionary index entry is 2 bytes
                int num_entries = (res_offset - dict_index_offset) / 2;

                dict_offsets.resize(num_entries);
                buf.read(dict_index_offset, &dict_offsets[0], 2 * num_entries);
                
                // the bottom 3 bits of firstByteAfterUids stores the number of bits used for 
                // dictionary tokens as an offset from 3, e.g. if 2 is stored in these three bits 
                // then the number of bits per dictionary token would be 3+2=5 - this allows a 
                // range of 3-11 bits per dictionary token (the maximum number of dictionary 
                // tokens therefore ranging from 8-2048) - the spec currently only supports 5-9
                num_of_bits_use_for_dict_token = 3 + (file_flag & 0x7);
                
                if ((num_res > 0) && (flags & first_res_generated_bit_array_of_res_contains_compressed_unicode)) {
                    // Just in case
                    // Decompress resource 0 to get the unicode array
                    unicode_flag_array.resize(size_of_largest_resource_when_uncompressed);
                    decompress(&unicode_flag_array[0], size_of_largest_resource_when_uncompressed, 0);
                }
            }
        } else {
            // We read a barebone format, nothing compressed
            // Symbian said that this format is likely to be used with non-ROM, since ROM has to be small
            buf.read(buf.size() - 2, &res_index_offset, 2);

            // The last index on the resource index table was pointing to start of index
            // It's not a valid one
            num_res = static_cast<std::uint16_t>((buf.size() - res_index_offset) / 2 - 1);

            if ((num_res > 0) && (flags & potentially_have_compressed_unicode_data)) {
                int length_of_bit_array_in_bytes = (num_res + 7) / 8;
                unicode_flag_array.resize(length_of_bit_array_in_bytes);

                buf.read(19, &unicode_flag_array[0], length_of_bit_array_in_bytes);
            }

            resource_offsets.resize(num_res);
            buf.read(res_index_offset, &resource_offsets[0], 2 * num_res);
            buf.read(res_index_offset, &res_offset, 2);

            res_data.resize(res_index_offset - res_offset);

            buf.read(res_offset, &res_data[0], static_cast<std::uint32_t>(res_data.size()));
        }

        // Done with the header
    }

    bool rsc_file::own_res_id(const int res_id) {
        // Abadon the offset right now
        // int offset = res_id & 0xfffff000;

        int res_index = res_id & 0x00000fff - 1;
        int number_of_res = num_res;

        if (flags & generate_rss_sig_for_first_user_res) {
            ++number_of_res;
        }

        if (flags & first_res_generated_bit_array_of_res_contains_compressed_unicode) {
            --number_of_res;
        }

        return (res_index >= 0) && (res_index < number_of_res);
    }
    
    std::vector<std::uint8_t> rsc_file::read(const int res_id) {
        if (!own_res_id(res_id)) {
            LOG_ERROR("RSC file doesn't own the resource id: 0x{:X}", res_id);
            return std::vector<std::uint8_t>{};
        }

        // First resource has UID 0x001, not 000
        int res_index = (res_id & 0xFFF) - 1;

        if (flags & generate_rss_sig_for_first_user_res) {
            if (res_index > 0) {
                --res_index;
            } else {
                std::vector<std::uint8_t> signatures_raw;
                signatures_raw.resize(8);

                std::uint32_t *sig = reinterpret_cast<std::uint32_t*>(&signatures_raw[0]);
                sig[0] = 4;
                sig[1] = ((uids.uid2 << 12) | 1);

                return signatures_raw;
            }
        }

        if (flags & first_res_generated_bit_array_of_res_contains_compressed_unicode) {
            res_index++;
        }

        std::vector<std::uint8_t> data;
        data.resize(size_of_largest_resource_when_uncompressed);

        int err_code = decompress(&data[0], size_of_largest_resource_when_uncompressed, res_index);

        if (err_code < 0) {
            LOG_ERROR("RSC file decompress encounters error: 0x{:X}", err_code);
            return std::vector<std::uint8_t>{};
        }

        // Returns value is the number of bytes readed
        data.resize(err_code);

        if (!is_resource_contains_unicode(res_index, flags & first_res_generated_bit_array_of_res_contains_compressed_unicode)) {
            return data;
        }

        // Need to decompress unicode
        std::vector<std::uint8_t> stage2_data;
        stage2_data.resize(size_of_largest_resource_when_uncompressed);

        int index = 0;
        int written = 0;

        for (bool decompress_run = true; ; decompress_run = !decompress_run) {
            int runlen = data[index];
            if (runlen & 0x80) {
                ++index;

                if (index >= size_of_largest_resource_when_uncompressed) {
                    return std::vector<std::uint8_t>{};
                }

                runlen &= ~0x80;
                runlen <<= 8;
                runlen |= data[index];
            }

            ++index;

            if (runlen > 0) {
                int start_off = common::max(0, written);
                std::uint8_t *append_data = &stage2_data[start_off];

                if (decompress_run) {
                    // If it's odd, we should append a padding byte but valid
                    if (reinterpret_cast<std::uint64_t>(append_data) & 0x01) {
                        *(append_data++) = 0xAB;
                        written += 1;
                    }

                    common::unicode_expander expander;

                    // Bytes should not multiply, since read_char16 already adds number of bytes by 2
                    written += expander.expand(&data[index], runlen, append_data, size_of_largest_resource_when_uncompressed - written);
                    index += runlen;
                } else {
                    std::memcpy(append_data, &data[index], runlen);
                    written += runlen;
                    index += runlen;
                }
            }

            if (index >= static_cast<int>(data.size())) {
                break;
            }
        }

        stage2_data.resize(written);
        return stage2_data;
    }

    std::uint32_t rsc_file::get_uid(const int idx) {
        switch (idx) {
        case 1: {
            return uids.uid1;
        }

        case 2: {
            return uids.uid2;
        }

        case 3: {
            return uids.uid3;
        }

        default: {
            break;
        }
        }

        assert(false);
        return 0;
    }

    rsc_file::rsc_file(common::ro_buf_stream &buf)
        : flags(0) {
        read_header_and_resource_index(buf);
    }
}