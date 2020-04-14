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

#pragma once

#include <epoc/services/akn/skin/bitmap_store.h>
#include <epoc/services/akn/skin/common.h>
#include <epoc/services/akn/skin/skn.h>
#include <epoc/services/fbs/fbs.h>

#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1::kernel {
    class chunk;
}

namespace eka2l1::epoc {
    struct skn_file;
    struct skn_bitmap_info;
    struct skn_color_table;
    struct skn_image_table;
    struct skn_effect_queue;
    struct skn_effect;
    struct skn_effect_parameter;

    enum class akn_skin_chunk_area_base_offset {
        item_def_area_base = 0,
        item_def_area_allocated_size = 1,
        item_def_area_current_size = 2,
        data_area_base = 3,
        data_area_allocated_size = 4,
        data_area_current_size = 5,
        filename_area_base = 6,
        filename_area_allocated_size = 7,
        filename_area_current_size = 8,
        gfx_area_base = 9,
        gfx_area_allocated_size = 10,
        gfx_area_current_size = 11,
        item_def_hash_base = 12,
        item_def_hash_allocated_size = 13,
        item_def_hash_current_size = 14,
        base_offset_end
    };

    enum akn_skin_chunk_maintainer_flag {
        akn_skin_chunk_maintainer_lookup_use_linked_list = 1 << 0 ///< Use linked list for lookup definition. Available on S^3.
    };

    /**
     * Maximum length of a filename in shared chunk.
     */
    constexpr std::uint32_t AKN_SKIN_SERVER_MAX_FILENAME_LENGTH = 512;

    /**
     * Maximum bytes of a filename in shared chunk.
     */
    constexpr std::uint32_t AKN_SKIN_SERVER_MAX_FILENAME_BYTES = AKN_SKIN_SERVER_MAX_FILENAME_LENGTH * 2;

    /**
     * \brief The AKN skin server's chunk maintainer.
     * 
     * The shared chunk stores skin data, which is accessible to all clients which has access to the chunk.
     * 
     * The chunk maintainer manages the shared chunk's area and read/write operation to the shared chunk, like
     * storing skin or image data, or writing filenames.
     */
    class akn_skin_chunk_maintainer {
        struct akn_skin_chunk_area {
            akn_skin_chunk_area_base_offset base_;
            std::size_t gran_off_;
            std::int64_t gran_size_;
        };

        kernel::chunk *shared_chunk_;
        std::size_t current_granularity_off_;
        std::size_t max_size_gran_;

        std::size_t granularity_;

        // Remember to sort this
        std::vector<akn_skin_chunk_area> areas_;

        std::uint32_t level_;
        std::uint32_t flags_;

        std::unique_ptr<akn_skin_bitmap_store> bitmap_store_;

        /**
         * \brief    Get maximum number of filename that the filename area can hold.
         * \returns  uint32_t(-1) if the filename area doesn't exist, else the expected value.
         */
        const std::uint32_t maximum_filename();

        /**
         * \brief    Get current number of filename that the filename area holds.
         * \returns  uint32_t(-1) if the filename area doesn't exist, else the expected value.
         */
        const std::uint32_t current_filename_count();

        /**
         * \brief Add a new area in the chunk. 
         *
         * Note: The first chunk size will be subtracted with the chunk header size, as more area will be added.
         * In the case the first area memory will run out if another area got added, this will return false.
         * 
         * \param offset_type             Area type. Must be from enum member "*_base".
         * \param allocated_size_gran     Size to be allocated for the chunk, in granularity.
         *                                If this is negative, the size is equals to granularity divide this.
         * 
         * \returns false if one of these conditions are not met, or insufficient memory.
         */
        bool add_area(const akn_skin_chunk_area_base_offset offset_type, const std::int64_t allocated_size_gran);

        /**
         * \brief   Find an existing area and return the area's info.
         * 
         * \param   area_type   The kind of area. Must be from enum member "*_base".
         * 
         * \returns Nullptr if the condition doesn't meet or area doesn't exist, else returns pointer to
         *          the area info.
         * 
         * \see     get_area_size
         * \see     get_area_base
         */
        akn_skin_chunk_area *get_area_info(const akn_skin_chunk_area_base_offset area_type);

        /**
         * \brief   Import bitmap definition.
         * 
         * \param   info Info of the bitmap.
         * \returns True on success.
         */
        bool import_bitmap(const skn_bitmap_info &info);

        /**
         * \brief   Import color table to skin definition chunk.
         * 
         * \param   table Info of the table.
         * \returns True on success.
         */
        bool import_color_table(const skn_color_table &table);

        /**
         * \brief   Import image table to skin defintion chunk.
         * 
         * \param   table Info of the table.
         * \returns True on success.
         */
        bool import_image_table(const skn_image_table &table);

        /**
         * \brief   Import effect queue into skin defintion chunk.
         * 
         * \param   queue The queue.
         * \returns True on success.
         */
        bool import_effect_queue(const skn_effect_queue &queue);

    public:
        explicit akn_skin_chunk_maintainer(kernel::chunk *shared_chunk, const std::size_t granularity,
            const std::uint32_t flags = 0);

        /**
         * \brief   Get an area's max size
         * 
         * \param   area_type       The kind of area. Must be from enum member "*_base".
         * \param   paper_calc      Calculate area max size based on info given to add_area before. Disable by default 
         * \returns size_t(-1) if an area doesn't exist, else returns the maximum size of area, in bytes.
         * 
         * \see     get_area_base
         */
        const std::size_t get_area_size(const akn_skin_chunk_area_base_offset area_type, const bool paper_calc = false);

        /**
         * \brief   Get an area's current size
         * 
         * \param   area_type   The kind of area. Must be from enum member "*_base".
         * \returns size_t(-1) if an area doesn't exist, else returns the current size of area, in bytes.
         * 
         * \see     set_area_current_size
         */
        const std::size_t get_area_current_size(const akn_skin_chunk_area_base_offset area_type);

        /**
         * \brief   Set an area's current size.
         * 
         * This is not safe to use directly, but is still being exposed public anyway. Use at your own risks.
         * 
         * \param   area_type   The kind of area. Must be from enum member "*_base".
         * \param   new_size    The new size of the area.
         * 
         * \returns True if success.
         * 
         * \see     get_area_current_size
         */
        bool set_area_current_size(const akn_skin_chunk_area_base_offset area_type, const std::uint32_t new_size);

        /**
         * \brief Update the data in data section.
         * 
         * \param new_data The data to be copied to data section.
         * \param old_data The previous related data if available.
         * \param new_size The new size of the data.
         * \param old_size The old size of data if available, else 0.
         * 
         * \returns Offset of the data in the data section.
         */
        std::int32_t update_data(const std::uint8_t *new_data, std::uint8_t *old_data, const std::size_t new_size, const std::size_t old_size);

        /**
         * \brief   Get a pointer to an existing area's base.
         * 
         * \param   area_type              The kind of area. Must be from enum member "*_base".
         * \param   offset_from_begin      Optional parameter, a pointer to the number that will contains the offset of the area
         *                               starting from the beginning of the shared memory chunk.
         * 
         * \returns Nullptr if the condition doesn't meet or area doesn't exist, else returns base pointer to
         *          the area.
         * 
         * \see     get_area_size
         */
        void *get_area_base(const akn_skin_chunk_area_base_offset area_type, std::uint64_t *offset_from_begin = nullptr);

        /**
         * \brief Update a filename in the shared chunk.
         * 
         * The function first will tries to lookup the filename ID. If the ID doesn't exist yet, the area will be expanded,
         * and the filename will be added to the end of the list.
         * 
         * The second situation is when the filename ID has already exist. In this case, the filename in the chunk will be
         * updated, or we can say, overwritten with the provided filename string.
         * 
         * Each filename has 512 bytes reserved for it:
         * - The first four bytes store the ID of the filename. This is used for lookup.
         * - The left are for filename base and the filename.
         * 
         * \param filename_id    The ID of filename.
         * \param filename       Filename to be updated.
         * \param filename_base  Base path to the filename.
         * 
         * \returns True if success.
         * 
         * \see     update_defintion
         */
        bool update_filename(const std::uint32_t filename_id, const std::u16string &filename, const std::u16string &filename_base);

        /**
         * \brief Lookup filename and returns its data offset from filename area.
         * 
         * The function iterates through filename area trying to find a filename with given ID.
         * In case the filename is found, the functions returns offset of the filename from filename area.
         * 
         * If failed, the function returns -1.
         * 
         * \param   filename_id The ID of the filename.
         * \returns -1 on failure.
         */
        std::int32_t get_filename_offset_from_id(const std::uint32_t filename_id);

        /**
         * \brief   Update the hash of a definition.
         * \param   def The defintion.
         * 
         * \returns True on success.
         */
        bool update_definition_hash(epoc::akns_item_def *def, const std::int32_t index);

        /**
         * \brief   Get item defintion index with its ID.
         * 
         * \param   id The ID of the defintion.
         * \returns The index of the item in item def chunk if available, else -1.
         */
        std::int32_t get_item_definition_index(const epoc::pid &id);

        /**
         * \brief   Update defintion of an item.
         * 
         * \param   def               The definition struct.
         * \param   data              Data that comes with the definition.
         * \param   data_size         Size of the data.
         * \param   old_data_size     Old size of the data.
         * 
         * \returns True on success.
         * 
         * \see     update_filename
         */
        bool update_definition(const epoc::akns_item_def &def, const void *data, const std::size_t data_size,
            const std::size_t old_data_size);

        /**
         * \brief   Import SKN parse results to the chunk.
         * 
         * \param   skn Reference to the SKN parser.
         * 
         * \returns True on success.
         */
        bool import(skn_file &skn, const std::u16string &filename_base);

        /**
         * \brief    Get an item's definition with given ID.
         * 
         * \param    id The ID to lookup item's definition.
         * \returns  Pointer to the item definition if available, else nullptr.
         */
        akns_item_def *get_item_definition(const epoc::pid &id);

        bool store_scalable_gfx(const pid item_id, const skn_layout_info layout_info, fbsbitmap *bmp, fbsbitmap *mask);

        const std::uint32_t level() const {
            return level_;
        }
    };
}