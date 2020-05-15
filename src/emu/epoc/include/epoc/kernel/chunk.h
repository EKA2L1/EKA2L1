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

#pragma once

#include <epoc/kernel/kernel_obj.h>
#include <epoc/mem.h>
#include <epoc/mem/chunk.h>
#include <epoc/ptr.h>

#include <common/types.h>

#include <cstdint>
#include <memory>
#include <string>

namespace eka2l1 {
    using address = uint32_t;

    class memory_system;
    class kernel_system;

    namespace kernel {
        class process;
    }

    namespace mem {
        struct mem_model_chunk;
    }

    using process_ptr = kernel::process *;

    /*! \brief Contains kernel objects implementation. */
    namespace kernel {
        /*! \brief The chunk type.
		*/
        enum class chunk_type {
            disconnected,
            double_ended,
            normal
        };

        enum class chunk_access {
            global,
            local,
            code,
            rom,
            kernel_mapping
        };

        enum class chunk_attrib {
            none,
            anonymous
        };

        /*! \brief A chunk.
		 *
		 * Chunk is a big space of reserved memory. In that reserved memory, you can commit and decommit thing.
		*/
        class chunk : public kernel_obj {
            // The reversed region that the chunk can commit to
            mem::mem_model_chunk *mmc_impl_;
            std::unique_ptr<mem::mem_model_chunk> mmc_impl_unq_;

            memory_system *mem;

            bool is_heap;

            chunk_type type;

        public:
            explicit chunk(kernel_system *kern, memory_system *mem)
                : kernel_obj(kern)
                , mem(mem) {
                obj_type = kernel::object_type::chunk;
            }

            explicit chunk(kernel_system *kern, memory_system *mem, kernel::process *own_process, std::string name, address bottom,
                const address top, const size_t max_grow_size, prot protection, chunk_type type, chunk_access access,
                chunk_attrib attrib, const bool is_heap = false, const address force_addr = 0, void *force_host_map = 0);

            ~chunk() = default;

            /*! \brief Commit to a disconnected chunk. 
			 *
			 * Offset and size SHOULD be aligned with the page size, 
             * else this will results unwanted behavior. E.g commit(page_size - 1, 2), should commit both
             * the first and second page, since the offset is at the first page, and the commit contains
             * both last and first byte of two page. This will results two WHOLE pages being allocated. 
			 * \param offset The offset to commit to.
			 * \param size The size to commit.
			 * \returns false if failed to commit.
			*/
            bool commit(uint32_t offset, size_t size);

            /*! \brief Decommit to a disconnected chunk.
			 *
             * Offset and size SHOULD be aligned with the page size.
             * The reason is same as for commit.
			 * \param offset The offset to decommit.
			 * \param size The size to decommit.
			 * \returns false if failed to decommit.
			*/
            bool decommit(uint32_t offset, size_t size);

            /*! \brief Destroy the chunk */
            void destroy() override;

            void open_to(process *own) override;

            /*! \brief Get the base of the chunk. */
            ptr<uint8_t> base(process *pr);

            /*! \brief Adjust the chunk size.
			 * \param adj_size The size of the new adjusted chunk.
			 * \returns false if adjust failed.
			*/
            bool adjust(size_t adj_size);

            /*! \brief Adjust the size by setting the top and bottom of a chunk
            */
            bool adjust_de(size_t top, size_t bottom);

            /*! \brief The definition of this is blurry and uncleared. However,
             * afaik it commits to the top with size 
             */
            bool allocate(size_t size);

            kernel::process *get_own_process() {
                return reinterpret_cast<kernel::process *>(owner);
            }

            bool is_chunk_heap() const {
                return is_heap;
            }

            const std::size_t max_size() const;
            const std::size_t committed() const;

            void *host_base();
        };
    }
}
