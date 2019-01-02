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

#include <arm/arm_interface.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/chunkyseri.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>
#include <epoc/mem.h>
#include <epoc/kernel/libmanager.h>
#include <epoc/kernel/scheduler.h>
#include <epoc/kernel/thread.h>
#include <epoc/loader/romimage.h>
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
        destroy_all_processes();
    }

    bool kernel_system::run_thread(uint32_t handle) {
        auto res = get_thread_by_handle(handle);

        if (!res) {
            return false;
        }

        thr_sch->schedule(res);

        return true;
    }

    bool kernel_system::run_process(uint32_t handle) {
        auto res = get_process(handle);

        if (!res) {
            return false;
        }

        return res->run();
    }

    thread_ptr kernel_system::crr_thread() {
        return thr_sch->current_thread();
    }

    process_ptr kernel_system::crr_process() {
        return thr_sch->current_process();
    }

    uint32_t kernel_system::spawn_new_process(const std::string &path, const std::string &name,
		kernel::owner_type owner) {
        std::u16string path16 = common::utf8_to_ucs2(path);
        symfile f = io->open_file(path16, READ_MODE | BIN_MODE);

        if (!f) {
            return 0xFFFFFFFF;
        }

        auto temp = loader::parse_e32img(f, true);

        if (!temp) {
            f->seek(0, eka2l1::file_seek_mode::beg);
            auto romimg = loader::parse_romimg(f, mem);

            if (romimg) {
                loader::romimg_ptr img_ptr = libmngr->load_romimg(path16, false);
                libmngr->open_romimg(img_ptr);

                uint32_t h = create_process(img_ptr->header.uid3, name, path16, u"", img_ptr,
                    static_cast<kernel::process_priority>(img_ptr->header.priority), owner);

                run_process(h);

                f->close();

                return h;
            }

            f->close();
            return false;
        }

        auto res2 = libmngr->load_e32img(path16);

        if (!res2) {
            return 0xFFFFFFFF;
        }

        libmngr->open_e32img(res2);
        libmngr->patch_hle();

        uint32_t h = create_process(res2->header.uid3, name, path16, u"", res2,
            static_cast<kernel::process_priority>(res2->header.priority), owner);
        run_process(h);

        f->close();

        return h;
    }

    uint32_t kernel_system::spawn_new_process(uint32_t uid, kernel::owner_type owner) {
        return spawn_new_process(mngr->get_package_manager()->get_app_executable_path(uid),
            mngr->get_package_manager()->get_app_name(uid), owner);
    }

    bool kernel_system::destroy_all_processes() {
        SYNCHRONIZE_ACCESS;

        for (auto &obj : objects) {
            if (obj && obj->get_object_type() == kernel::object_type::process) {
                obj.reset();
            }
        }

        return true;
    }

    process_ptr kernel_system::get_process(std::string &name) {
        auto pr_find = std::find_if(objects.begin(), objects.end(),
            [&](auto &obj) {
                if (obj && obj->get_object_type() == kernel::object_type::process && obj->name() == name) {
                    return true;
                }

                return false;
            });

        if (pr_find == objects.end()) {
            return nullptr;
        }

        return std::reinterpret_pointer_cast<kernel::process>(*pr_find);
    }

    process_ptr kernel_system::get_process(uint32_t handle) {
        kernel_obj_ptr obj = get_kernel_obj(handle);

        if (!obj || obj->get_object_type() != kernel::object_type::process) {
            return nullptr;
        }

        return std::reinterpret_pointer_cast<kernel::process>(obj);
    }

    void kernel_system::prepare_reschedule() {
        sys->prepare_reschedule();
    }

    uint32_t kernel_system::create_handle_lastest(kernel::owner_type owner) {
        switch (owner) {
        case kernel::owner_type::kernel:
            return kernel_handles.add_object(objects.back());

        case kernel::owner_type::process:
            return crr_process()->process_handles.add_object(objects.back());

        case kernel::owner_type::thread:
            return crr_thread()->thread_handles.add_object(objects.back());

        default:
            return -1;
        }
    }

    uint32_t kernel_system::create_chunk(std::string name, const address bottom, const address top, const size_t size, prot protection,
        kernel::chunk_type type, kernel::chunk_access access, kernel::chunk_attrib attrib, kernel::owner_type owner) {
        chunk_ptr new_chunk = std::make_shared<kernel::chunk>(this, mem, crr_process(), name, bottom, top, size,
            protection, type, access, attrib);

        {
            SYNCHRONIZE_ACCESS;
            objects.push_back(std::move(new_chunk));
        }

        return create_handle_lastest(owner);
    }

    uint32_t kernel_system::create_mutex(std::string name, bool init_locked,
        kernel::owner_type own,
        kernel::access_type access) {
        mutex_ptr new_mutex = std::make_shared<kernel::mutex>(this, name, init_locked, access);

        {
            SYNCHRONIZE_ACCESS;
            objects.push_back(std::move(new_mutex));
        }

        return create_handle_lastest(own);
    }

    uint32_t kernel_system::create_sema(std::string sema_name,
        int32_t init_count,
        kernel::owner_type own_type,
        kernel::access_type access) {
        sema_ptr new_sema = std::make_shared<kernel::semaphore>(this, sema_name, init_count, access);

        {
            SYNCHRONIZE_ACCESS;
            objects.push_back(std::move(new_sema));
        }

        return create_handle_lastest(own_type);
    }

    uint32_t kernel_system::create_timer(std::string name, kernel::owner_type owner,
        kernel::access_type access) {
        timer_ptr new_timer = std::make_shared<kernel::timer>(this, timing, name, access);

        {
            SYNCHRONIZE_ACCESS;
            objects.push_back(std::move(new_timer));
        }

        return create_handle_lastest(owner);
    }

    uint32_t kernel_system::create_change_notifier(kernel::owner_type owner) {
        change_notifier_ptr new_nof = std::make_shared<kernel::change_notifier>(this);

        {
            SYNCHRONIZE_ACCESS;
            objects.push_back(std::move(new_nof));
        }

        return create_handle_lastest(owner);
    }

    uint32_t kernel_system::create_thread(kernel::owner_type owner, process_ptr own_pr, kernel::access_type access,
        const std::string &name, const address epa, const size_t stack_size,
        const size_t min_heap_size, const size_t max_heap_size,
        bool initial,
        ptr<void> usrdata,
        ptr<void> allocator,
        kernel::thread_priority pri) {
        thread_ptr new_thread = std::make_shared<kernel::thread>(this, mem, timing, own_pr, access,
            name, epa, stack_size, min_heap_size, max_heap_size, initial, usrdata, allocator, pri);

        {
            SYNCHRONIZE_ACCESS;
            objects.push_back(std::move(new_thread));
        }

        return create_handle_lastest(owner);
    }

    uint32_t kernel_system::create_server(std::string name) {
        server_ptr new_server = std::make_shared<service::server>(sys, name);

        {
            SYNCHRONIZE_ACCESS;
            objects.push_back(std::move(new_server));
        }

        return create_handle_lastest(kernel::owner_type::kernel);
    }

    uint32_t kernel_system::create_session(server_ptr cnn_svr, int async_slots) {
        session_ptr new_session = std::make_shared<service::session>(this, cnn_svr, async_slots);

        {
            SYNCHRONIZE_ACCESS;
            objects.push_back(std::move(new_session));
        }

        return create_handle_lastest(kernel::owner_type::thread);
    }

    uint32_t kernel_system::create_prop(kernel::owner_type owner) {
        property_ptr new_prop = std::make_shared<service::property>(this);

        {
            SYNCHRONIZE_ACCESS;
            objects.push_back(std::move(new_prop));
        }

        return create_handle_lastest(owner);
    }

    uint32_t kernel_system::create_process(uint32_t uid,
        const std::string &process_name, const std::u16string &exe_path,
        const std::u16string &cmd_args, loader::e32img_ptr &img,
        const kernel::process_priority pri, kernel::owner_type own) {
        process_ptr pr = std::make_shared<kernel::process>(this, mem, uid, process_name, exe_path, cmd_args, img,
            pri);

        // Create a POSIX server for the process
        std::shared_ptr<eka2l1::posix_server> ps_srv = std::make_shared<eka2l1::posix_server>(sys, pr);
        add_custom_server(std::move(ps_srv));

        get_thread_by_handle(pr->primary_thread)->owning_process(pr);

        {
            SYNCHRONIZE_ACCESS;
            objects.push_back(std::move(pr));
        }

        return create_handle_lastest(own);
    }

    uint32_t kernel_system::create_process(uint32_t uid,
        const std::string &process_name, const std::u16string &exe_path,
        const std::u16string &cmd_args, loader::romimg_ptr &img,
        const kernel::process_priority pri, kernel::owner_type own) {
        process_ptr pr = std::make_shared<kernel::process>(this, mem, uid, process_name, exe_path, cmd_args, img,
            pri);

        // Create a POSIX server for the process
        std::shared_ptr<eka2l1::posix_server> ps_srv = std::make_shared<eka2l1::posix_server>(sys, pr);
        add_custom_server(std::move(ps_srv));

        get_thread_by_handle(pr->primary_thread)->owning_process(pr);

        {
            SYNCHRONIZE_ACCESS;
            objects.push_back(std::move(pr));
        }

        return create_handle_lastest(own);
    }

    uint32_t kernel_system::create_library(const std::string &name, loader::romimg_ptr &img,
        kernel::owner_type own) {
        library_ptr lib = std::make_shared<kernel::library>(this, name, img);

        {
            SYNCHRONIZE_ACCESS;
            objects.push_back(std::move(lib));
        }

        return create_handle_lastest(own);
    }

    uint32_t kernel_system::create_library(const std::string &name, loader::e32img_ptr &img,
        kernel::owner_type own) {
        library_ptr lib = std::make_shared<kernel::library>(this, name, img);

        {
            SYNCHRONIZE_ACCESS;
            objects.push_back(std::move(lib));
        }

        return create_handle_lastest(own);
    }

    ipc_msg_ptr kernel_system::create_msg(kernel::owner_type owner) {
        auto slot_free = std::find_if(msgs.begin(), msgs.end(),
            [](auto slot) { return !slot || slot->free; });

        if (slot_free != msgs.end()) {
            ipc_msg_ptr msg
                = std::make_shared<ipc_msg>(crr_thread());

            if (!*slot_free) {
                *slot_free = std::move(msg);
            }

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

        auto obj_ite = std::find_if(objects.begin(), objects.end(), [=](kernel_obj_ptr &obj2) {
            return obj && obj2 && (obj2->unique_id() == obj->unique_id());   
        });

        if (obj_ite != objects.end()) {
            objects.erase(obj_ite);
            return true;
        }

        return false;
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

    thread_ptr kernel_system::get_thread_by_handle(uint32_t handle) {
        kernel_obj_ptr obj = get_kernel_obj(handle);

        if (!obj || obj->get_object_type() != kernel::object_type::thread) {
            return nullptr;
        }

        return std::reinterpret_pointer_cast<kernel::thread>(obj);
    }

    kernel_obj_ptr kernel_system::get_kernel_obj(uint32_t handle) {
        if (handle == 0xFFFF8000) {
            return crr_process();
        } else if (handle == 0xFFFF8001) {
            return crr_thread();
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

    kernel_obj_ptr kernel_system::get_kernel_obj_by_id(std::uint32_t id) {
        SYNCHRONIZE_ACCESS;

        auto res = std::find_if(objects.begin(), objects.end(),
            [=](kernel_obj_ptr obj) { return obj && (obj->unique_id() == id); });

        if (res != objects.end()) {
            return *res;
        }

        return nullptr;
    }

    thread_ptr kernel_system::get_thread_by_name(const std::string &name) {
        SYNCHRONIZE_ACCESS;
        
        auto thr_find = std::find_if(objects.begin(), objects.end(),
            [&](auto &obj) {
                if (obj && obj->get_object_type() == kernel::object_type::thread && obj->name() == name) {
                    return true;
                }

                return false;
            });

        if (thr_find == objects.end()) {
            return nullptr;
        }

        return std::reinterpret_pointer_cast<kernel::thread>(*thr_find);
    }

    std::vector<thread_ptr> kernel_system::get_all_thread_own_process(process_ptr pr) {
        std::vector<thread_ptr> thr_list;

        SYNCHRONIZE_ACCESS;

        for (kernel_obj_ptr &obj : objects) {
            if (obj && obj->get_object_type() == kernel::object_type::thread) {
                thread_ptr thr = std::reinterpret_pointer_cast<kernel::thread>(obj);

                if (thr->own_process == pr) {
                    thr_list.push_back(thr);
                }
            }
        }

        return thr_list;
    }

    std::vector<process_ptr> kernel_system::get_all_processes() {
        std::vector<process_ptr> process_list;
        SYNCHRONIZE_ACCESS;

        for (kernel_obj_ptr &obj : objects) {
            if (obj && obj->get_object_type() == kernel::object_type::process) {
                process_ptr pr = std::reinterpret_pointer_cast<kernel::process>(obj);
                process_list.push_back(pr);
            }
        }

        return process_list;
    }

    void kernel_system::free_msg(ipc_msg_ptr msg) {
        msg->free = true;
    }

    /*! \brief Completely destroy a message. */
    void kernel_system::destroy_msg(ipc_msg_ptr msg) {
        (msgs.begin() + msg->id)->reset();
    }

    std::vector<process_ptr> kernel_system::get_process_list() {
        std::vector<process_ptr> processes;
        
        SYNCHRONIZE_ACCESS;

        for (const auto &obj: objects) {
            if (obj && obj->get_object_type() == kernel::object_type::process) {
                processes.push_back(std::reinterpret_pointer_cast<kernel::process>(obj));
            }
        }

        return processes;
    }

    server_ptr kernel_system::get_server_by_name(const std::string name) {
        SYNCHRONIZE_ACCESS;

        auto svr_find = std::find_if(objects.begin(), objects.end(),
            [&](auto &obj) {
                if (obj && obj->get_object_type() == kernel::object_type::server && obj->name() == name) {
                    return true;
                }

                return false;
            });

        if (svr_find == objects.end()) {
            return nullptr;
        }

        return std::reinterpret_pointer_cast<service::server>(*svr_find);
    }

    server_ptr kernel_system::get_server(uint32_t handle) {
        SYNCHRONIZE_ACCESS;

        kernel_obj_ptr obj = get_kernel_obj(handle);

        if (!obj || obj->get_object_type() != kernel::object_type::server) {
            return nullptr;
        }

        return std::reinterpret_pointer_cast<service::server>(obj);
    }

    session_ptr kernel_system::get_session(uint32_t handle) {
        kernel_obj_ptr obj = get_kernel_obj(handle);

        if (!obj || obj->get_object_type() != kernel::object_type::session) {
            return nullptr;
        }

        return std::reinterpret_pointer_cast<service::session>(obj);
    }

    property_ptr kernel_system::get_prop(int cagetory, int key) {
        SYNCHRONIZE_ACCESS;

        auto prop_res = std::find_if(objects.begin(), objects.end(),
            [=](const auto &prop) {
                if (prop && prop->get_object_type() == kernel::object_type::prop) {
                    property_ptr real_prop = std::reinterpret_pointer_cast<service::property>(prop);

                    if (real_prop->first == cagetory && real_prop->second == key) {
                        return true;
                    }
                }

                return false;
            });

        if (prop_res == objects.end()) {
            return property_ptr(nullptr);
        }

        return std::reinterpret_pointer_cast<service::property>(*prop_res);
    }

    property_ptr kernel_system::get_prop(uint32_t handle) {
        SYNCHRONIZE_ACCESS;

        kernel_obj_ptr obj = get_kernel_obj(handle);

        if (!obj || obj->get_object_type() != kernel::object_type::prop) {
            return nullptr;
        }

        return std::reinterpret_pointer_cast<service::property>(obj);
    }

    uint32_t kernel_system::mirror(thread_ptr own_thread, uint32_t handle, kernel::owner_type owner) {
        kernel_obj_ptr target_obj;
        kernel::handle_inspect_info info = kernel::inspect_handle(handle);

        if (handle == 0xFFFF8000) {
            target_obj = crr_process();
        } else if (handle == 0xFFFF8001) {
            target_obj = crr_thread();
        } else {
            target_obj = get_kernel_obj(handle);
        }

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

    uint32_t kernel_system::open_handle(kernel_obj_ptr obj, kernel::owner_type owner) {
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

    void kernel_system::processing_requests() {
        std::vector<server_ptr> svrs;

        {
            SYNCHRONIZE_ACCESS;

            for (auto &obj : objects) {
                if (obj && obj->get_object_type() == kernel::object_type::server) {
                    server_ptr svr = std::reinterpret_pointer_cast<service::server>(obj);

                    if (svr->is_hle()) {
                        svrs.push_back(svr);
                    }
                }
            }
        }

        for (server_ptr &svr : svrs) {
            svr->process_accepted_msg();
        }
    }

    uint32_t kernel_system::next_uid() const {
        ++uid_counter;
        return uid_counter.load();
    }

    std::optional<find_handle> kernel_system::find_object(const std::string &name, int start, kernel::object_type type) {
        find_handle handle_find_info;
        SYNCHRONIZE_ACCESS;

        if (start >= objects.size()) {
            return std::optional<find_handle>{};
        }

        const auto &obj = std::find_if(objects.begin() + start, objects.end(),
            [&](kernel_obj_ptr obj) { return obj && (obj->name() == name) && (obj->get_object_type() == type); });

        if (obj != objects.end()) {
            handle_find_info.index = static_cast<int>(obj - objects.begin());
            handle_find_info.object_id = (*obj)->unique_id();

            return handle_find_info;
        }

        return std::optional<find_handle>{};
    }

    bool kernel_system::should_terminate() {
        return thr_sch->should_terminate();
    }

    void kernel_system::do_state_of(common::chunkyseri &seri, const kernel::object_type t) {
        for (auto &o: objects) {
            if (o->get_object_type() == t) {
                o->do_state(seri);
            }
        }
    }
    
    struct kernel_info {
        std::uint32_t total_chunks;
        std::uint32_t total_mutex;
        std::uint32_t total_semaphore;
        std::uint32_t total_thread;
        std::uint32_t total_timer;
        std::uint32_t total_prop;
        std::uint32_t total_process;
    };

    void kernel_system::do_state(common::chunkyseri &seri) {
        auto s = seri.section("Kernel", 1);

        if (!s) {
            return;
        }

        kernel_info info;

        if (seri.get_seri_mode() == common::SERI_MODE_WRITE) {
            for (const auto &obj: objects) {
                switch (obj->get_object_type()) {
                case kernel::object_type::process: {
                    info.total_process++;
                    break;
                }

                case kernel::object_type::chunk: {
                    info.total_chunks++;
                    break;
                }

                case kernel::object_type::mutex: {
                    info.total_mutex++;
                    break;
                }

                case kernel::object_type::sema: {
                    info.total_semaphore++;
                    break;
                }

                case kernel::object_type::thread: {
                    info.total_thread++;
                    break;
                }

                case kernel::object_type::timer: {
                    info.total_timer++;
                    break;
                }

                case kernel::object_type::prop: {
                    info.total_prop++;
                    break;
                }

                default: {
                    LOG_ERROR("Unsupported save object");
                    return;
                }
                }
            }
        }

        seri.absorb(info.total_process);
        seri.absorb(info.total_chunks);
        seri.absorb(info.total_mutex);
        seri.absorb(info.total_semaphore);
        seri.absorb(info.total_thread);
        seri.absorb(info.total_timer);
        seri.absorb(info.total_prop);
        
        // First, loading all the neccessary info first
        // Create placeholder object
        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            for (std::uint32_t i = 0; i < info.total_process; i++) {
                auto pr = std::make_shared<kernel::process>(this, mem);
                objects.push_back(std::reinterpret_pointer_cast<kernel::kernel_obj>(std::move(pr)));
            }

            for (std::uint32_t i = 0; i < info.total_chunks; i++) {
                auto c = std::make_shared<kernel::chunk>(this, mem);
                objects.push_back(std::reinterpret_pointer_cast<kernel::kernel_obj>(std::move(c)));
            }

            for (std::uint32_t i = 0; i < info.total_mutex; i++) {
                auto c = std::make_shared<kernel::mutex>(this);
                objects.push_back(std::reinterpret_pointer_cast<kernel::kernel_obj>(std::move(c)));
            }
            
            for (std::uint32_t i = 0; i < info.total_semaphore; i++) {
                auto c = std::make_shared<kernel::semaphore>(this);
                objects.push_back(std::reinterpret_pointer_cast<kernel::kernel_obj>(std::move(c)));
            }
            
            for (std::uint32_t i = 0; i < info.total_thread; i++) {
                auto c = std::make_shared<kernel::thread>(this, mem, timing);
                objects.push_back(std::reinterpret_pointer_cast<kernel::kernel_obj>(std::move(c)));
            }
        }

        auto do_state_base_kernel_obj_for_type = [&](kernel::object_type obj, std::uint32_t max_count) {
            std::uint32_t count = 0;

            for (auto &o: objects) {
                if (o->get_object_type() == kernel::object_type::process) {
                    o->kernel_obj::do_state(seri);
                    count++;
                }

                if (count == max_count) {
                    count = 0;
                    break;
                }
            }
        };

        do_state_base_kernel_obj_for_type(kernel::object_type::process, info.total_process);
        do_state_base_kernel_obj_for_type(kernel::object_type::chunk, info.total_chunks);
        do_state_base_kernel_obj_for_type(kernel::object_type::mutex, info.total_mutex);
        do_state_base_kernel_obj_for_type(kernel::object_type::sema, info.total_semaphore);
        do_state_base_kernel_obj_for_type(kernel::object_type::thread, info.total_thread);
        do_state_base_kernel_obj_for_type(kernel::object_type::timer, info.total_timer);
        do_state_base_kernel_obj_for_type(kernel::object_type::timer, info.total_prop);

        do_state_of(seri, kernel::object_type::process);
        do_state_of(seri, kernel::object_type::chunk);
        do_state_of(seri, kernel::object_type::mutex);
        do_state_of(seri, kernel::object_type::sema);
        do_state_of(seri, kernel::object_type::thread);
        do_state_of(seri, kernel::object_type::timer);
        do_state_of(seri, kernel::object_type::prop);

        std::uint32_t uidc = uid_counter.load();
        seri.absorb(uidc);

        if (seri.get_seri_mode() == common::SERI_MODE_WRITE) {
            uid_counter = uidc;
        }
    }
}
