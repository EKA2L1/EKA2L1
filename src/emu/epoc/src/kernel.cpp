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
#include <algorithm>
#include <atomic>
#include <queue>
#include <thread>

#include <arm/arm_analyser.h>
#include <arm/arm_interface.h>

#include <common/buffer.h>
#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <common/virtualmem.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>
#include <epoc/kernel/libmanager.h>
#include <epoc/kernel/scheduler.h>
#include <epoc/kernel/thread.h>
#include <epoc/loader/romimage.h>
#include <epoc/mem.h>
#include <epoc/ptr.h>
#include <epoc/services/posix/posix.h>
#include <epoc/vfs.h>

#include <epoc/loader/e32img.h>
#include <epoc/loader/romimage.h>

#include <epoc/services/init.h>

#include <common/time.h>
#include <manager/manager.h>

namespace eka2l1 {
    void kernel_system::init(system *esys, timing_system *timing_sys, manager_system *mngrsys,
        memory_system *mem_sys, io_system *io_sys, hle::lib_manager *lib_sys, arm::arm_interface *cpu) {
        timing = timing_sys;
        mngr = mngrsys;
        mem = mem_sys;
        libmngr = lib_sys;
        io = io_sys;
        sys = esys;

        base_time = common::get_current_time_in_microseconds_since_1ad();

        thr_sch = std::make_shared<kernel::thread_scheduler>(this, timing, *cpu);
        rom_map = nullptr;

        kernel_handles = kernel::object_ix(this, kernel::handle_array_owner::kernel);
        service::init_services(sys);

        // Create real time IPC event
        realtime_ipc_signal_evt = timing->register_event("RealTimeIpc", [this](std::uint64_t userdata, std::uint64_t cycles_late) {
            kernel::thread *thr = get_by_id<kernel::thread>(static_cast<kernel::uid>(userdata));
            assert(thr);

            thr->signal_request();
        });
    }

    void kernel_system::shutdown() {
        if (rom_map) {
            common::unmap_file(rom_map);
        }

        rom_map = nullptr;
        thr_sch.reset();

        // Delete one by one in order. Do not change the order
        servers.clear();
        sessions.clear();
        timers.clear();
        mutexes.clear();
        semas.clear();
        change_notifiers.clear();
        props.clear();
        prop_refs.clear();
        chunks.clear();
        threads.clear();
        processes.clear();
        libraries.clear();
        codesegs.clear();
        message_queues.clear();
    }

    kernel::thread *kernel_system::crr_thread() {
        return thr_sch->current_thread();
    }

    kernel::process *kernel_system::crr_process() {
        return thr_sch->current_process();
    }

    void kernel_system::prepare_reschedule() {
        sys->prepare_reschedule();
    }

    ipc_msg_ptr kernel_system::create_msg(kernel::owner_type owner) {
        auto slot_free = std::find_if(msgs.begin(), msgs.end(),
            [](auto slot) { return !slot || slot->free; });

        if (slot_free != msgs.end()) {
            if (!*slot_free) {
                ipc_msg_ptr msg = std::make_shared<ipc_msg>(crr_thread());
                *slot_free = std::move(msg);
            }

            slot_free->get()->own_thr = crr_thread();
            slot_free->get()->free = false;
            slot_free->get()->id = static_cast<std::uint32_t>(slot_free - msgs.begin());

            return *slot_free;
        }

        return nullptr;
    }

    ipc_msg_ptr kernel_system::get_msg(int handle) {
        if (msgs.size() <= handle) {
            return nullptr;
        }

        return msgs[handle];
    }

    bool kernel_system::destroy(kernel_obj_ptr obj) {
        SYNCHRONIZE_ACCESS;

        switch (obj->get_object_type()) {
#define OBJECT_SEARCH(obj_type, obj_map)                                                                         \
    case kernel::object_type::obj_type: {                                                                        \
        auto res = std::lower_bound(obj_map.begin(), obj_map.end(), obj, [&](const auto &lhs, const auto &rhs) { \
            return lhs->unique_id() < rhs->unique_id();                                                          \
        });                                                                                                      \
        if (res == obj_map.end())                                                                                \
            return false;                                                                                        \
        (*res)->destroy();                                                                                       \
        obj_map.erase(res);                                                                                      \
        return true;                                                                                             \
    }

            OBJECT_SEARCH(mutex, mutexes)
            OBJECT_SEARCH(sema, semas)
            OBJECT_SEARCH(chunk, chunks)
            OBJECT_SEARCH(thread, threads)
            OBJECT_SEARCH(process, processes)
            OBJECT_SEARCH(change_notifier, change_notifiers)
            OBJECT_SEARCH(library, libraries)
            OBJECT_SEARCH(codeseg, codesegs)
            OBJECT_SEARCH(server, servers)
            OBJECT_SEARCH(prop, props)
            OBJECT_SEARCH(prop_ref, prop_refs)
            OBJECT_SEARCH(session, sessions)
            OBJECT_SEARCH(timer, timers)
            OBJECT_SEARCH(msg_queue, message_queues)

#undef OBJECT_SEARCH

        default:
            break;
        }

        return false;
    }

    // We can support also ELF!
    process_ptr kernel_system::spawn_new_process(const std::u16string &path, const std::u16string &cmd_arg, const kernel::uid promised_uid3,
        const std::uint32_t stack_size) {
        auto imgs = libmngr->try_search_and_parse(path);

        if (!imgs.first && !imgs.second) {
            return nullptr;
        }

        std::string path8 = common::ucs2_to_utf8(path);
        std::string process_name = eka2l1::filename(path8);

        /* Create process through kernel system. */
        process_ptr pr = create<kernel::process>(mem, process_name, path, cmd_arg);

        if (!pr) {
            return nullptr;
        }

        codeseg_ptr cs = nullptr;

        std::uint32_t new_stack_size = (stack_size == 0) ? 0xFFFFFFFF : stack_size;
        kernel::process_priority pri = kernel::process_priority::high;
        std::uint32_t heap_min = 0;
        std::uint32_t heap_max = 0;

        if (imgs.first) {
            auto &eimg = imgs.first;

            if (promised_uid3 != 0 && eimg->header.uid3 != promised_uid3) {
                return nullptr;
            }

            // Load and add to cache
            if (stack_size == 0) {
                new_stack_size = imgs.first->header.stack_size;
            } else {
                new_stack_size = std::min<std::uint32_t>(imgs.first->header.stack_size, stack_size);
            }

            pri = static_cast<kernel::process_priority>(imgs.first->header.priority);
            heap_min = imgs.first->header.heap_size_min;
            heap_max = imgs.first->header.heap_size_max;

            cs = libmngr->load_as_e32img(*eimg, &(*pr), path);
        }

        // Rom image
        if (!cs && imgs.second) {
            if (promised_uid3 != 0 && imgs.second->header.uid3 != promised_uid3) {
                return nullptr;
            }

            if (stack_size == 0) {
                new_stack_size = imgs.second->header.stack_size;
            } else {
                new_stack_size = std::min<std::uint32_t>(imgs.second->header.stack_size, stack_size);
            }

            pri = static_cast<kernel::process_priority>(imgs.second->header.priority);
            heap_min = imgs.second->header.heap_minimum_size;
            heap_max = imgs.second->header.heap_maximum_size;

            cs = libmngr->load_as_romimg(*imgs.second, &(*pr), path);
        }

        if (!cs) {
            destroy(pr);
            return nullptr;
        }

        LOG_TRACE("Spawned process: {}, entry point = 0x{:X}", process_name, cs->get_code_run_addr(&(*pr)));

        pr->construct_with_codeseg(cs, new_stack_size, heap_min, heap_max, pri);
        return pr;
    }

    int kernel_system::close(std::uint32_t handle) {
        kernel::handle_inspect_info info = kernel::inspect_handle(handle);

        if (info.no_close) {
            return -1;
        }

        if (info.handle_array_local) {
            return crr_thread()->thread_handles.close(handle);
        }

        if (info.handle_array_kernel) {
            return kernel_handles.close(handle);
        }

        return crr_process()->process_handles.close(handle);
    }

    kernel_obj_ptr kernel_system::get_kernel_obj_raw(uint32_t handle) {
        if (handle == 0xFFFF8000) {
            return reinterpret_cast<kernel::kernel_obj *>(get_by_id<kernel::process>(
                crr_process()->unique_id()));
        } else if (handle == 0xFFFF8001) {
            return reinterpret_cast<kernel::kernel_obj *>(get_by_id<kernel::thread>(
                crr_thread()->unique_id()));
        }

        kernel::handle_inspect_info info = kernel::inspect_handle(handle);

        if (info.handle_array_local) {
            return crr_thread()->thread_handles.get_object(handle);
        }

        if (info.handle_array_kernel) {
            return kernel_handles.get_object(handle);
        }

        return crr_process()->process_handles.get_object(handle);
    }

    void kernel_system::free_msg(ipc_msg_ptr msg) {
        if (msg->locked()) {
            return;
        }

        msg->free = true;
    }

    /*! \brief Completely destroy a message. */
    void kernel_system::destroy_msg(ipc_msg_ptr msg) {
        (msgs.begin() + msg->id)->reset();
    }

    property_ptr kernel_system::get_prop(int cagetory, int key) {
        SYNCHRONIZE_ACCESS;

        auto prop_res = std::find_if(props.begin(), props.end(),
            [=](const auto &prop_obj) {
                property_ptr prop = reinterpret_cast<property_ptr>(prop_obj.get());
                if (prop->first == cagetory && prop->second == key) {
                    return true;
                }

                return false;
            });

        if (prop_res == props.end()) {
            return property_ptr(nullptr);
        }

        return reinterpret_cast<property_ptr>(prop_res->get());
    }

    kernel::handle kernel_system::mirror(kernel::thread *own_thread, kernel::handle handle, kernel::owner_type owner) {
        kernel_obj_ptr target_obj = get_kernel_obj_raw(handle);
        kernel::handle_inspect_info info = kernel::inspect_handle(handle);

        kernel::handle h = INVALID_HANDLE;

        switch (owner) {
        case kernel::owner_type::kernel:
            return kernel_handles.add_object(target_obj);

        case kernel::owner_type::thread: {
            if (!own_thread) {
                return INVALID_HANDLE;
            }

            h = own_thread->thread_handles.add_object(target_obj);
            break;
        }

        case kernel::owner_type::process: {
            h = own_thread->owning_process()->process_handles.add_object(target_obj);
            break;
        }

        default:
            break;
        }

        if (h != INVALID_HANDLE) {
            target_obj->open_to(own_thread->owning_process());
        }

        return h;
    }

    uint32_t kernel_system::mirror(kernel_obj_ptr obj, kernel::owner_type owner) {
        kernel::handle h = INVALID_HANDLE;

        switch (owner) {
        case kernel::owner_type::kernel:
            return kernel_handles.add_object(obj);

        case kernel::owner_type::thread: {
            h = crr_thread()->thread_handles.add_object(obj);
            break;
        }

        case kernel::owner_type::process: {
            h = crr_process()->process_handles.add_object(obj);
            break;
        }

        default:
            return INVALID_HANDLE;
        }

        if (h != INVALID_HANDLE) {
            obj->open_to(crr_process());
        }

        return h;
    }

    kernel::handle kernel_system::open_handle(kernel_obj_ptr obj, kernel::owner_type owner) {
        return open_handle_with_thread(crr_thread(), obj, owner);
    }

    kernel::handle kernel_system::open_handle_with_thread(kernel::thread *thr, kernel_obj_ptr obj, kernel::owner_type owner) {
        kernel::handle h = INVALID_HANDLE;

        switch (owner) {
        case kernel::owner_type::kernel:
            return kernel_handles.add_object(obj);

        case kernel::owner_type::thread: {
            h = thr->thread_handles.add_object(obj);
            break;
        }

        case kernel::owner_type::process: {
            h = thr->owning_process()->process_handles.add_object(obj);
            break;
        }

        default:
            break;
        }

        if (h != INVALID_HANDLE) {
            obj->open_to(thr->owning_process());
        }

        return h;
    }

    uint32_t kernel_system::next_uid() const {
        ++uid_counter;
        return uid_counter.load();
    }

    void kernel_system::setup_new_process(process_ptr pr) {
        // TODO(pent0): Hope i can enable this server again, it's currently buggy
        // std::unique_ptr<service::server> ps_srv = std::make_unique<eka2l1::posix_server>(sys, pr);
        // add_custom_server(ps_srv);
    }

    codeseg_ptr kernel_system::pull_codeseg_by_ep(const address ep) {
        auto res = std::find_if(codesegs.begin(), codesegs.end(), [=](const auto &cs) -> bool {
            return reinterpret_cast<codeseg_ptr>(cs.get())->get_entry_point(nullptr) == ep;
        });

        if (res == codesegs.end()) {
            return nullptr;
        }

        return reinterpret_cast<codeseg_ptr>(res->get());
    }

    codeseg_ptr kernel_system::pull_codeseg_by_uids(const kernel::uid uid0, const kernel::uid uid1,
        const kernel::uid uid2) {
        auto res = std::find_if(codesegs.begin(), codesegs.end(), [=](const auto &cs) -> bool {
            return reinterpret_cast<codeseg_ptr>(cs.get())->get_uids() == std::make_tuple(uid0, uid1, uid2);
        });

        if (res == codesegs.end()) {
            return nullptr;
        }

        return reinterpret_cast<codeseg_ptr>(res->get());
    }

    std::optional<find_handle> kernel_system::find_object(const std::string &name, int start, kernel::object_type type, const bool use_full_name) {
        find_handle handle_find_info;
        SYNCHRONIZE_ACCESS;

        switch (type) {
#define OBJECT_SEARCH(obj_type, obj_map)                                                       \
    case kernel::object_type::obj_type: {                                                      \
        auto res = std::find_if(obj_map.begin() + start, obj_map.end(), [&](const auto &rhs) { \
            std::string to_compare = "";                                                       \
            if (use_full_name) {                                                               \
                rhs->full_name(to_compare);                                                    \
            } else {                                                                           \
                to_compare = rhs->name();                                                      \
            }                                                                                  \
            return name == to_compare;                                                         \
        });                                                                                    \
        if (res == obj_map.end())                                                              \
            return std::nullopt;                                                               \
        handle_find_info.index = static_cast<int>(std::distance(obj_map.begin(), res));        \
        handle_find_info.object_id = (*res)->unique_id();                                      \
        return handle_find_info;                                                               \
    }

            OBJECT_SEARCH(mutex, mutexes)
            OBJECT_SEARCH(sema, semas)
            OBJECT_SEARCH(chunk, chunks)
            OBJECT_SEARCH(thread, threads)
            OBJECT_SEARCH(process, processes)
            OBJECT_SEARCH(change_notifier, change_notifiers)
            OBJECT_SEARCH(library, libraries)
            OBJECT_SEARCH(codeseg, codesegs)
            OBJECT_SEARCH(server, servers)
            OBJECT_SEARCH(prop, props)
            OBJECT_SEARCH(prop_ref, prop_refs)
            OBJECT_SEARCH(session, sessions)
            OBJECT_SEARCH(timer, timers)
            OBJECT_SEARCH(msg_queue, message_queues)

#undef OBJECT_SEARCH

        default:
            break;
        }

        return std::nullopt;
    }

    bool kernel_system::should_terminate() {
        return thr_sch->should_terminate();
    }

    bool kernel_system::map_rom(const mem::vm_address addr, const std::string &path) {
        rom_map = common::map_file(path, prot::read_write, 0, true);
        const std::size_t rom_size = common::file_size(path);

        if (!rom_map) {
            return false;
        }

        LOG_TRACE("Rom mapped to address: 0x{:x}", reinterpret_cast<std::uint64_t>(rom_map));

        // Don't care about the result as long as it's not null.
        kernel::chunk *rom_chunk = create<kernel::chunk>(mem, nullptr, "ROM", 0, rom_size,
            static_cast<address>(rom_size), prot::read_write_exec, kernel::chunk_type::normal, kernel::chunk_access::rom,
            kernel::chunk_attrib::none, false, addr, rom_map);

        if (!rom_chunk) {
            LOG_ERROR("Can't create ROM chunk!");

            common::unmap_file(rom_map);
            return false;
        }

        return true;
    }

    struct kernel_info {
        std::uint32_t total_chunks{ 0 };
        std::uint32_t total_mutex{ 0 };
        std::uint32_t total_semaphore{ 0 };
        std::uint32_t total_thread{ 0 };
        std::uint32_t total_timer{ 0 };
        std::uint32_t total_prop{ 0 };
        std::uint32_t total_process{ 0 };

        kernel_info() {}
    };

    void kernel_system::do_state(common::chunkyseri &seri) {
        auto s = seri.section("Kernel", 1);

        if (!s) {
            return;
        }
    }

    std::uint64_t kernel_system::home_time() {
        return base_time + timing->get_global_time_us();
    }
}
