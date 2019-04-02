/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <cstddef>
#include <cstdint>
#include <vector>

namespace eka2l1::common {
    class ro_stream;
}

namespace eka2l1::loader {
    struct mif_header_v2 {
        int uid;
        int version;
        int offset;         ///< Offset of index section.
        int array_len;      ///< Total entry count.
    };

    struct mif_header_v3: public mif_header_v2 {
        int index_dll_uid;
    };

    struct mif_index {
        int offset;
        int len;
    };

    /**
     * \brief Machine-readable file parser.
     */
    struct mif_file {
        mif_header_v3 header_;
        std::vector<mif_index> idxs_;

        common::ro_stream *stream_;

        explicit mif_file(common::ro_stream *stream);

        /**
         * \brief Parse MIF headers and indexes.
         * \returns True on success. 
         */
        bool do_parse();
        
        /**
         * \brief Read an MIF data entry.
         * 
         * Client using this must gurantees that the stream are still valid until the
         * copy is finished.
         * 
         * \param idx       The index of the target entry. Index is zero-based.
         * \param buf       Target dest buffer. Pass nullptr to this parameter to get the needed dest size.
         * \param dest_size Maximum size the target buffer can hold. On success, this will contains total bytes
         *                  written.
         * 
         * \returns false if read failed.
         */
        bool read_mif_entry(const std::size_t idx, std::uint8_t *buf, int &dest_size);
    };
}
