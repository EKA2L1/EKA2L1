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

#include <epoc/kernel.h>
#include <epoc/kernel/chunk.h>
#include <epoc/kernel/object_ix.h>

#include <common/chunkyseri.h>
#include <common/log.h>

#include <algorithm>

namespace eka2l1 {
    namespace kernel {
        handle_inspect_info inspect_handle(std::uint32_t handle) {
            handle_inspect_info info;

            if (handle >> 31 == 1) {
                // Special
                info.special = true;
                info.special_type = static_cast<special_handle_type>(handle);
            } else {
                info.special = false;
            }

            if (((handle >> 30) & 0xf) == 1) {
                info.handle_array_local = true;
            } else {
                info.handle_array_local = false;
            }

            // On emulator, use bit 29 to determine if it's a kernel handle
            if (handle & (1 << 29)) {
                info.handle_array_kernel = true;
            } else {
                info.handle_array_kernel = false;
            }

            info.no_close = handle & 0x8000;
            info.object_ix_next_instance = (handle >> 16) & 0b0001111111111111;
            info.object_ix_index = handle & 0x7FFF;

            return info;
        }

        std::uint32_t object_ix::make_handle(size_t index) {
            std::uint32_t handle = 0;

            handle |= next_instance << 16;
            handle |= index;

            if (owner == handle_array_owner::thread) {
                // If handle array owner is thread, the 30th bit must be 1
                handle |= 0x40000000;
            }

            if (owner == handle_array_owner::kernel) {
                handle |= (1 << 29);
            }

            handles.push_back(handle);

            return handle;
        }

        std::uint32_t object_ix::add_object(kernel_obj_ptr obj) {
            auto slot = std::find_if(objects.begin(), objects.end(),
                [](object_ix_record record) { return record.free; });

            if (slot != objects.end()) {
                next_instance++;
                std::uint32_t ret_handle = make_handle(slot - objects.begin());

                slot->associated_handle = ret_handle;
                slot->free = false;
                slot->object = obj;

                obj->increase_access_count();

                return ret_handle;
            }

            return INVALID_HANDLE;
        }

        std::uint32_t object_ix::last_handle() {
            if (!handles.size()) {
                return 0;
            }

            std::uint32_t last = handles.back();
            handles.pop_back();

            return last;
        }

        std::uint32_t object_ix::duplicate(std::uint32_t handle) {
            handle_inspect_info info = inspect_handle(handle);

            if (!info.handle_array_local && owner == handle_array_owner::process) {
                return INVALID_HANDLE;
            }

            kernel_obj_ptr obj = get_object(handle);

            if (!obj) {
                return INVALID_HANDLE;
            }

            return add_object(obj);
        }

        kernel_obj_ptr object_ix::get_object(std::uint32_t handle) {
            handle_inspect_info info = inspect_handle(handle);

            if (info.object_ix_index < objects.size()) {
                return objects[info.object_ix_index].object;
            }

            LOG_WARN("Can't find object with handle: 0x{:x}", handle);

            return nullptr;
        }

        int object_ix::close(std::uint32_t handle) {
            handle_inspect_info info = inspect_handle(handle);
            int ret_value = 0;

            if (info.object_ix_index < objects.size()) {
                kernel_obj_ptr obj = objects[info.object_ix_index].object;

                if (!obj) {
                    return -1;
                }

                obj->decrease_access_count();
                obj->close();

                if (obj->get_access_count() <= 0 && obj->get_object_type() != object_type::process && obj->get_object_type() != object_type::thread) {
                    if (obj->get_object_type() == object_type::chunk) {
                        chunk_ptr c = reinterpret_cast<kernel::chunk *>(obj);

                        // This is a force hack signaling the closing one is chunk heap, which means the
                        // thread is in destruction, and detach needed
                        if (c->is_chunk_heap()) {
                            ret_value = 1;
                        }
                    }

                    kern->destroy(obj);
                }

                objects[info.object_ix_index].free = true;

                return ret_value;
            }

            return -1;
        }

        object_ix::object_ix(kernel_system *kern, handle_array_owner owner)
            : kern(kern)
            , owner(owner)
            , next_instance(0)
            , uid(kern->next_uid()) {}

        void object_ix::do_state(common::chunkyseri &seri) {
            auto s = seri.section("ObjectIx", 1);

            if (!s) {
                return;
            }

            seri.absorb(uid);
            seri.absorb(next_instance);
            seri.absorb(owner);

            std::stack<std::uint16_t> slot_used;
            std::uint32_t slot_count = 0;

            if (seri.get_seri_mode() == common::SERI_MODE_WRITE) {
                for (std::size_t i = 0; i < objects.size(); i++) {
                    if (!objects[i].free) {
                        slot_count++;
                        slot_used.push(static_cast<std::uint16_t>(i));
                    }
                }
            }

            seri.absorb(slot_count);

            std::uint32_t next_slot_use = 0;

            for (std::uint32_t i = 0; i < slot_count; i++) {
                std::uint32_t obj_id = 0;

                if (seri.get_seri_mode() == common::SERI_MODE_WRITE) {
                    next_slot_use = slot_used.top();
                    slot_used.pop();

                    obj_id = objects[next_slot_use].object->unique_id();
                }

                seri.absorb(next_slot_use);
                seri.absorb(obj_id);
                seri.absorb(objects[next_slot_use].associated_handle);

                if (seri.get_seri_mode() == common::SERI_MODE_READ) {
                    // TODO
                    //objects[next_slot_use].object = kern->get_kernel_obj_raw(obj_id);
                    objects[next_slot_use].free = false;
                }
            }

            // Hey, we need to save last thread handle too
            seri.absorb_container(handles);
        }
    }
}