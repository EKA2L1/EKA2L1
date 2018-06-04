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
#include <common/log.h>
#include <common/random.h>
#include <common/cvt.h>
#include <kernel/chunk.h>
#include <core_kernel.h>
#include <core_mem.h>

namespace eka2l1 {
    namespace kernel {
        chunk::chunk(kernel_system* kern, memory_system* mem, std::string name, address bottom, const address top, const size_t max_size, prot protection,
            chunk_type type, chunk_access access, chunk_attrib attrib, kernel::owner_type owner_type, kernel::uid owner)
            : kernel_obj(kern, name, owner_type, owner)
            , type(type)
            , access(access)
            , attrib(attrib)
            , max_size(max_size)
            , protection(protection)
            , kern(kern)
            , mem(mem) {
            if (attrib == chunk_attrib::anonymous) {
                // Don't use the name given by that, it's anonymous omg!!!
                obj_name = "anonymous" + common::to_string(eka2l1::random());
            } 

            if (access == chunk_access::local && name == "") {
                obj_name = "local" + common::to_string(eka2l1::random());
            }

            if (access == chunk_access::code) {
                obj_name = "code" + common::to_string(eka2l1::random());
            }

            address new_top = top;
            address new_bottom = bottom;

            if (type == chunk_type::normal) {
                // Adjust the top and bottom. Later
                size_t init_commit_size = new_top - new_bottom;
 
                new_top = max_size;
                new_bottom = new_top - init_commit_size;
            }

            address range_beg = 0;
            address range_end = 0;

            switch (access) {
                case chunk_access::local: {
                    range_beg = LOCAL_DATA;
                    range_end = DLL_STATIC_DATA;
                    break;
                }
            
                case chunk_access::global: {
                    range_beg = GLOBAL_DATA;
                    range_end = RAM_DRIVE; // Drive D
                    break;
                }

                case chunk_access::code: {
                    range_beg = RAM_CODE_ADDR;
                    range_end = ROM;
                    break;
                }
            }

            chunk_base = ptr<uint8_t>(mem->chunk_range(range_beg, range_end, new_bottom, new_top, max_size, protection).ptr_address());
            
            this->top = new_top;
            this->bottom = new_bottom;

            LOG_INFO("Chunk created: {}, id: {}, bottom: {}, top: {}, type: {}, access: {}{}", obj_name, obj_id, new_bottom, new_top,
                (type == chunk_type::normal ? "normal" : (type == chunk_type::disconnected ? "disconnected" : "double ended")), 
                (access == chunk_access::local ? "local" : (access == chunk_access::code ? "code " : "global")), 
                (attrib == chunk_attrib::anonymous ? ", anonymous" : ""));
        }

        chunk::~chunk() {
            mem->unchunk(ptr<void>(chunk_base.ptr_address()), max_size);
        }

        bool chunk::commit(uint32_t offset, size_t size) {
            if (type != kernel::chunk_type::disconnected) {
                return false;
            }

            mem->commit(ptr<void>(chunk_base.ptr_address() + offset), size);

            if (offset + size > top) {
               top = offset;
            }
          
            if (offset < bottom) {
                bottom = offset;
            }    

            commited_size += size;

            return true;
        }

        bool chunk::decommit(uint32_t offset, size_t size) {
            if (type != kernel::chunk_type::disconnected) {
                return false;
            }

            mem->decommit(ptr<void>(chunk_base.ptr_address() + offset), size);

            top = common::max(top, (uint32_t)(offset + size));
            bottom = common::min(bottom, offset);

            commited_size -= size;

            return true;
        }

        bool chunk::adjust(size_t adj_size) {
            if (type == kernel::chunk_type::disconnected) {
                return false;
            }

            mem->commit(ptr<void>(chunk_base.ptr_address() + bottom), adj_size);
            top = bottom + adj_size;

            return true;
        }
    }
}
