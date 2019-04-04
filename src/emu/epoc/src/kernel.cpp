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

#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>

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

#include <epoc/services/init.h>

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

        thr_sch = std::make_shared<kernel::thread_scheduler>(this, timing, *cpu);

        kernel_handles = kernel::object_ix(this, kernel::handle_array_owner::kernel);
        service::init_services(sys);
    }

    void kernel_system::shutdown() {
        thr_sch.reset();
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
            OBJECT_SEARCH(session, sessions)
            OBJECT_SEARCH(timer, timers)

#undef OBJECT_SEARCH

        default:
            break;
        }

        return false;
    }

    process_ptr kernel_system::spawn_new_process(const kernel::uid uid) {
        return spawn_new_process(
            common::utf8_to_ucs2(mngr->get_package_manager()->get_app_executable_path(uid)));
    }

    // We can support also ELF!
    process_ptr kernel_system::spawn_new_process(const std::u16string &path,
        const std::u16string &cmd_arg, const kernel::uid promised_uid3,
        const std::uint32_t stack_size) {
        auto imgs = libmngr->try_search_and_parse(path);

        if (!imgs.first && !imgs.second) {
            return nullptr;
        }

        codeseg_ptr cs = nullptr;

        std::string path8 = common::ucs2_to_utf8(path);
        std::string process_name = eka2l1::filename(path8);

        if (imgs.first) {
            auto &eimg = imgs.first;

            if (promised_uid3 != 0 && eimg->header.uid3 != promised_uid3) {
                return nullptr;
            }

            // Load and add to cache
            if (!cs) {
                cs = libmngr->load_as_e32img(*eimg, path);
            }

            /* Create process through kernel system. */
            process_ptr pr = create<kernel::process>(
                mem, eimg->header.uid3, process_name, path,
                cmd_arg, cs, stack_size ? std::min(stack_size, eimg->header.stack_size) : eimg->header.stack_size,
                eimg->header.heap_size_min, eimg->header.heap_size_max,
                static_cast<kernel::process_priority>(eimg->header.priority));

            if (pr) {
                LOG_TRACE("Spawned process: {}", process_name);
            }

            return pr;
        }

        if (promised_uid3 != 0 && imgs.second->header.uid3 != promised_uid3) {
            return nullptr;
        }

        // Rom image
        if (!cs) {
            cs = libmngr->load_as_romimg(*imgs.second, path);
        }

        auto &rimg = imgs.second;

        /* Create process through kernel system. */
        process_ptr pr = create<kernel::process>(
            mem, rimg->header.uid3, process_name, path,
            cmd_arg, cs,
            stack_size ? std::min(stack_size, static_cast<std::uint32_t>(rimg->header.stack_size)) : static_cast<std::uint32_t>(rimg->header.stack_size),
            rimg->header.heap_minimum_size, rimg->header.heap_maximum_size,
            static_cast<kernel::process_priority>(rimg->header.priority));

        if (pr) {
            LOG_TRACE("Spawned process: {}, entry point: 0x{:x}", process_name, rimg->header.entry_point);
        }

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
            return std::reinterpret_pointer_cast<kernel::kernel_obj>(get_by_id<kernel::process>(
                crr_process()->unique_id()));
        } else if (handle == 0xFFFF8001) {
            return std::reinterpret_pointer_cast<kernel::kernel_obj>(get_by_id<kernel::thread>(
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
            [=](const auto &prop) {
                if (prop->first == cagetory && prop->second == key) {
                    return true;
                }

                return false;
            });

        if (prop_res == props.end()) {
            return property_ptr(nullptr);
        }

        return *prop_res;
    }

    kernel::handle kernel_system::mirror(kernel::thread *own_thread, kernel::handle handle, kernel::owner_type owner) {
        kernel_obj_ptr target_obj = get_kernel_obj_raw(handle);
        kernel::handle_inspect_info info = kernel::inspect_handle(handle);

        switch (owner) {
        case kernel::owner_type::kernel:
            return kernel_handles.add_object(target_obj);

        case kernel::owner_type::thread: {
            if (!own_thread) {
                return 0xFFFFFFFF;
            }

            return own_thread->thread_handles.add_object(target_obj);
        }

        case kernel::owner_type::process: {
            return crr_process()->process_handles.add_object(target_obj);
        }

        default:
            return 0xFFFFFFFF;
        }
    }

    uint32_t kernel_system::mirror(kernel_obj_ptr obj, kernel::owner_type owner) {
        switch (owner) {
        case kernel::owner_type::kernel:
            return kernel_handles.add_object(obj);

        case kernel::owner_type::thread: {
            return crr_thread()->thread_handles.add_object(obj);
        }

        case kernel::owner_type::process: {
            return crr_process()->process_handles.add_object(obj);
        }

        default:
            return 0xFFFFFFFF;
        }
    }

    kernel::handle kernel_system::open_handle(kernel_obj_ptr obj, kernel::owner_type owner) {
        return open_handle_with_thread(crr_thread(), obj, owner);
    }

    kernel::handle kernel_system::open_handle_with_thread(kernel::thread *thr, kernel_obj_ptr obj, kernel::owner_type owner) {
        switch (owner) {
        case kernel::owner_type::kernel:
            return kernel_handles.add_object(obj);

        case kernel::owner_type::thread: {
            return thr->thread_handles.add_object(obj);
        }

        case kernel::owner_type::process: {
            return thr->owning_process()->process_handles.add_object(obj);
        }

        default:
            break;
        }

        return INVALID_HANDLE;
    }

    uint32_t kernel_system::next_uid() const {
        ++uid_counter;
        return uid_counter.load();
    }

    void kernel_system::setup_new_process(process_ptr pr) {
        std::shared_ptr<eka2l1::posix_server> ps_srv = std::make_shared<eka2l1::posix_server>(sys, pr);
        add_custom_server(std::move(std::reinterpret_pointer_cast<service::server>(ps_srv)));

        pr->primary_thread->owning_process(&(*pr));
    }

    codeseg_ptr kernel_system::pull_codeseg_by_ep(const address ep) {
        auto res = std::find_if(codesegs.begin(), codesegs.end(), [=](const auto &cs) -> bool {
            return cs->get_entry_point() == ep;
        });

        if (res == codesegs.end()) {
            return nullptr;
        }

        return *res;
    }

    codeseg_ptr kernel_system::pull_codeseg_by_uids(const kernel::uid uid0, const kernel::uid uid1,
        const kernel::uid uid2) {
        auto res = std::find_if(codesegs.begin(), codesegs.end(), [=](const auto &cs) -> bool {
            return cs->get_uids() == std::make_tuple(uid0, uid1, uid2);
        });

        if (res == codesegs.end()) {
            return nullptr;
        }

        return *res;
    }

    std::optional<find_handle> kernel_system::find_object(const std::string &name, int start, kernel::object_type type) {
        find_handle handle_find_info;
        SYNCHRONIZE_ACCESS;

        switch (type) {
#define OBJECT_SEARCH(obj_type, obj_map)                                                       \
    case kernel::object_type::obj_type: {                                                      \
        auto res = std::find_if(obj_map.begin() + start, obj_map.end(), [&](const auto &rhs) { \
            return name == rhs->name();                                                        \
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
            OBJECT_SEARCH(session, sessions)
            OBJECT_SEARCH(timer, timers)

#undef OBJECT_SEARCH

        default:
            break;
        }

        return std::nullopt;
    }

    bool kernel_system::should_terminate() {
        return thr_sch->should_terminate();
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
}
