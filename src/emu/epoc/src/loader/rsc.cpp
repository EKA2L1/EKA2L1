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

#include <epoc/loader/rsc.h>
#include <epoc/vfs.h>

namespace eka2l1::loader {
    /*
       The header format should be readed like this:
       - First 12 bytes should contain 3 UID type
       - The first UID will divide the RSC file into 3 kind:
          * 0x101f4a6b: This will potentially contains compressed unicode data, which is compressed using run-len
          * 0x101f5010: This will potentially contains compressed unicode data + will be compressed using dictionary algroithm
       - If it doesn't fall into 2 of these types, discard all of these read result.
       - Read the first 2 bytes, if it's equals to 4 than it is fallen into a format called "Calypso" and dictionary compressed.
    */

   /*
        Calypso:
            - Offset 0-> 2: Magic (4)
            - Offset 2-> 4: Number of resources in this file
            - Offset 4-> 5: ???
            - Offset 5-> 6: unk1
            - Offset 6-> 8: Offset of resource index
            - Offset 8-> 10: Size of largest resource when decompress
   */
    void rsc_file_read_stream::read_header_and_resource_index(symfile f) {
        std::uint32_t uid[3];
        f->read_file(&uid, 4, 3);

        switch (uid[0]) {
        case 0x101F4A6B: {
            flags |= potentially_have_compressed_unicode_data;

            f->seek(17, eka2l1::file_seek_mode::beg);
            f->read_file(&size_of_largest_resource_when_uncompressed, sizeof(size_of_largest_resource_when_uncompressed),
                1);

            break;
        }

        case 0x101F5010: {
            flags |= (potentially_have_compressed_unicode_data | dictionary_compressed);

            f->seek(17, eka2l1::file_seek_mode::beg);
            f->read_file(&size_of_largest_resource_when_uncompressed, sizeof(size_of_largest_resource_when_uncompressed),
                1);

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

            f->seek(8, file_seek_mode::beg);
            f->read_file(&size_of_largest_resource_when_uncompressed, sizeof(size_of_largest_resource_when_uncompressed),
                1);
        }

        if (flags & dictionary_compressed) {
            if (flags & calypso) {
                // We read it before
                num_res = uid[0] & 0xFFFF;

                f->seek(10, file_seek_mode::beg);
                f->read_file(&num_of_bits_use_for_dict_token, sizeof(num_of_bits_use_for_dict_token), 1);

                char unk1 = 0;

                f->seek(5, file_seek_mode::beg);
                f->read_file(&unk1, 1, 1);

                num_dir_entry = (1 << num_of_bits_use_for_dict_token) - unk1;

                f->seek(6, file_seek_mode::beg);
                f->read_file(&res_index_offset, sizeof(res_index_offset), 1);
            }
        }

        // Done with the header
    }
}