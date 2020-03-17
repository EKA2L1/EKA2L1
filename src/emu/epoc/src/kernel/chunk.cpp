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
#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/random.h>

#include <epoc/kernel.h>
#include <epoc/kernel/chunk.h>

#include <epoc/mem.h>
#include <epoc/mem/chunk.h>
#include <epoc/mem/process.h>

namespace eka2l1 {
    namespace kernel {
        chunk::chunk(kernel_system *kern, memory_system *mem, kernel::process *own_process, std::string name,
            address bottom, const address top, const size_t max_size, prot protection, chunk_type type, chunk_access chnk_access,
            chunk_attrib attrib, const bool is_heap, const address force_addr, void *force_host_map)
            : kernel_obj(kern, name, own_process, access_type::local_access)
            , mem(mem)
            , is_heap(is_heap)
            , type(type) {
            obj_type = object_type::chunk;
            mem::mem_model_chunk_creation_info create_info {};

            create_info.perm = protection;
            create_info.size = max_size;

            if (chnk_access == chunk_access::global) {
                access = access_type::global_access;
                create_info.flags |= mem::MEM_MODEL_CHUNK_REGION_USER_GLOBAL;
            }

            if (attrib == chunk_attrib::anonymous) {
                // Don't use the name given by that, it's anonymous omg!!!
                obj_name = "anonymous" + common::to_string(eka2l1::random());
            }

            if (chnk_access == chunk_access::local) {
                create_info.flags |= mem::MEM_MODEL_CHUNK_REGION_USER_LOCAL;

                if (name == "") {
                    obj_name = "local" + common::to_string(eka2l1::random());
                }
            }

            if (chnk_access == chunk_access::code) {
                obj_name = "code" + common::to_string(eka2l1::random());
                create_info.flags |= mem::MEM_MODEL_CHUNK_REGION_USER_CODE;
            }

            if (chnk_access == chunk_access::rom) {
                create_info.flags |= mem::MEM_MODEL_CHUNK_REGION_USER_ROM;
            }

            if (chnk_access == chunk_access::kernel_mapping) {
                create_info.flags |= mem::MEM_MODEL_CHUNK_REGION_KERNEL_MAPPING;
            }

            switch (type) {
            case chunk_type::disconnected: {
                create_info.flags |= mem::MEM_MODEL_CHUNK_TYPE_DISCONNECT;
                break;
            }

            case chunk_type::double_ended: {
                create_info.flags |= mem::MEM_MODEL_CHUNK_TYPE_DOUBLE_ENDED;
                break;
            }

            case chunk_type::normal: {
                create_info.flags |= mem::MEM_MODEL_CHUNK_TYPE_NORMAL;
                break;
            }

            default:
                break;
            }

            create_info.addr = force_addr;
            create_info.host_map = force_host_map;

            mmc_impl_ = nullptr;

            if (own_process) {
                own_process->get_mem_model()->create_chunk(mmc_impl_, create_info);
            } else {
                mmc_impl_unq_ = mem::make_new_mem_model_chunk(mem->get_mmu(), 0, mem::mem_model_type::multiple);
                mmc_impl_unq_->do_create(create_info);

                mmc_impl_ = mmc_impl_unq_.get();
            }

            mmc_impl_->adjust(bottom, top);

            LOG_INFO("Chunk created: {}, base: 0x{:x}, max size: 0x{:x} type: {}, access: {}{}", obj_name,
                mmc_impl_->base(), max_size, (type == chunk_type::normal ? "normal" : (type == chunk_type::disconnected ? "disconnected" : "double ended")),
                (chnk_access == chunk_access::local ? "local" : (chnk_access == chunk_access::code ? "code " : "global")),
                (attrib == chunk_attrib::anonymous ? ", anonymous" : ""));
        }

        void chunk::destroy() {
            if (!mmc_impl_unq_)
                get_own_process()->get_mem_model()->delete_chunk(mmc_impl_);
        }
    
        void chunk::open_to(process *own) {
            own->get_mem_model()->attach_chunk(mmc_impl_);
        }

        ptr<uint8_t> chunk::base() {
            return mmc_impl_->base();
        }

        const std::size_t chunk::max_size() const {
            return mmc_impl_->max();
        }

        const std::size_t chunk::committed() const {
            return mmc_impl_->committed();
        }

        bool chunk::commit(uint32_t offset, size_t size) {
            if (type != kernel::chunk_type::disconnected) {
                return false;
            }

            if (mmc_impl_->commit(offset, size) == 0) {
                return false;
            }

            return true;
        }

        bool chunk::decommit(uint32_t offset, size_t size) {
            if (type != kernel::chunk_type::disconnected) {
                return false;
            }
            
            mmc_impl_->decommit(offset, size);
            return true;
        }

        bool chunk::adjust(std::size_t adj_size) {
            if (type == kernel::chunk_type::disconnected) {
                return false;
            }

            return mmc_impl_->adjust(0xFFFFFFFF, static_cast<address>(adj_size));
        }

        bool chunk::adjust_de(size_t ntop, size_t nbottom) {
            if (type != kernel::chunk_type::double_ended) {
                return false;
            }

            return mmc_impl_->adjust(static_cast<address>(nbottom), static_cast<address>(ntop));
        }

        bool chunk::allocate(size_t size) {
            if (type != kernel::chunk_type::disconnected) {
                return 0;
            }

            return mmc_impl_->allocate(size);
        }

        void *chunk::host_base() {
            return mmc_impl_->host_base();
        }
    }
}
