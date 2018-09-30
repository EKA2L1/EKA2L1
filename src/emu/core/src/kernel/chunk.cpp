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
#include <common/cvt.h>
#include <common/log.h>
#include <common/random.h>

#include <core/core_kernel.h>
#include <core/core_mem.h>
#include <core/kernel/chunk.h>

namespace eka2l1 {
    namespace kernel {
        chunk::chunk(kernel_system *kern, memory_system *mem, process_ptr own_process, std::string name,
            address bottom, const address top, const size_t max_size, prot protection,
            chunk_type type, chunk_access chnk_access, chunk_attrib attrib)
            : kernel_obj(kern, name, access_type::local_access)
            , type(type)
            , caccess(chnk_access)
            , attrib(attrib)
            , max_size(max_size)
            , protection(protection)
            , kern(kern)
            , mem(mem)
            , own_process(own_process) {
            obj_type = object_type::chunk;

            if (caccess == chunk_access::global) {
                access = access_type::global_access;
            }

            if (attrib == chunk_attrib::anonymous) {
                // Don't use the name given by that, it's anonymous omg!!!
                obj_name = "anonymous" + common::to_string(eka2l1::random());
            }

            if (caccess == chunk_access::local && name == "") {
                obj_name = "local" + common::to_string(eka2l1::random());
            }

            if (caccess == chunk_access::code) {
                obj_name = "code" + common::to_string(eka2l1::random());
            }

            address new_top = top;
            address new_bottom = bottom;

            if (type == chunk_type::normal) {
                // Adjust the top and bottom. Later
                size_t init_commit_size = new_top - new_bottom;

                new_top = init_commit_size;
                new_bottom = 0;
            }

            address range_beg = 0;
            address range_end = 0;

            switch (caccess) {
            case chunk_access::local: {
                range_beg = local_data;
                range_end = dll_static_data;
                break;
            }

            case chunk_access::global: {
                if (kern->get_epoc_version() == epocver::epoc6) {
                    range_beg = shared_data_eka1;
                    range_end = shared_data_end_eka1;
                } else {
                    range_beg = shared_data;
                    range_end = ram_code_addr; // Drive D
                }

                break;
            }

            case chunk_access::code: {
                if (kern->get_epoc_version() == epocver::epoc6) {
                    range_beg = ram_code_addr_eka1;
                    range_end = ram_code_addr_eka1_end;
                } else {
                    range_beg = ram_code_addr;
                    range_end = rom;
                }

                break;
            }
            }

            chunk_base = ptr<uint8_t>(mem->chunk_range(range_beg, range_end, new_bottom,
                                             new_top, max_size, protection)
                                          .ptr_address());

            this->top = new_top;
            this->bottom = new_bottom;

            LOG_INFO("Chunk created: {}, base: 0x{:x}, max size: 0x{:x}, bottom: {}, top: {}, type: {}, access: {}{}", obj_name, chunk_base.ptr_address(),
                max_size, new_bottom, new_top,
                (type == chunk_type::normal ? "normal" : (type == chunk_type::disconnected ? "disconnected" : "double ended")),
                (caccess == chunk_access::local ? "local" : (caccess == chunk_access::code ? "code " : "global")),
                (attrib == chunk_attrib::anonymous ? ", anonymous" : ""));
        }

        void chunk::destroy() {
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

        bool chunk::adjust_de(size_t ntop, size_t nbottom) {
            if (type != kernel::chunk_type::double_ended) {
                return false;
            }

            if (ntop - nbottom > max_size) {
                return false;
            }

            top = ntop;
            bottom = nbottom;

            mem->commit(ptr<void>(chunk_base.ptr_address() + bottom), top - bottom);

            return true;
        }

        uint32_t chunk::allocate(size_t size) {
            commit(top, size);
            return top - size;
        }

        void chunk::write_object_to_snapshot(common::wo_buf_stream &stream) {
            kernel_obj::write_object_to_snapshot(stream);

            size_t ptr_addr = chunk_base.ptr_address();
            uint64_t pr_uid = own_process->unique_id();

            stream.write(&type, sizeof(type));
            stream.write(&attrib, sizeof(attrib));
            stream.write(&caccess, sizeof(caccess));
            stream.write(&max_size, sizeof(max_size));
            stream.write(&top, sizeof(top));
            stream.write(&bottom, sizeof(bottom));
            stream.write(&ptr_addr, sizeof(chunk_base.ptr_address()));
            stream.write(&pr_uid, sizeof(pr_uid));

            // Check the range
            page_table &pt = own_process->get_page_table();

            stream.write(&protection, sizeof(protection));

            for (size_t i = (chunk_base.ptr_address() + bottom) / mem->get_page_size();
                 i < (chunk_base.ptr_address() + top) / mem->get_page_size(); i++) {
                stream.write(&(pt.pointers[i][0]), mem->get_page_size());
            }
        }

        void chunk::do_state(common::ro_buf_stream &stream) {
            kernel_obj::do_state(stream);

            uint64_t uid_pr = 0;

            stream.read(&type, sizeof(type));
            stream.read(&attrib, sizeof(attrib));
            stream.read(&caccess, sizeof(caccess));
            stream.read(&max_size, sizeof(max_size));
            stream.read(&top, sizeof(top));
            stream.read(&bottom, sizeof(bottom));
            stream.read(&chunk_base, sizeof(chunk_base));
            stream.read(&uid_pr, sizeof(uid_pr));

            commited_size = top - bottom;

            stream.read(&protection, sizeof(protection));

            own_process = std::dynamic_pointer_cast<kernel::process>(
                kern->get_kernel_obj_by_id(uid_pr));

            page_table *old = mem->get_current_page_table();
            mem->set_current_page_table(own_process->get_page_table());

            mem->chunk(chunk_base.ptr_address(), bottom, top, max_size, protection);
            mem->set_current_page_table(*old);
        }
    }
}
