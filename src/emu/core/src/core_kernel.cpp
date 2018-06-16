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

#include <common/log.h>
#include <core_kernel.h>
#include <core_mem.h>
#include <hle/libmanager.h>
#include <kernel/scheduler.h>
#include <kernel/thread.h>
#include <manager/manager.h>
#include <ptr.h>
#include <vfs.h>

namespace eka2l1 {
    void kernel_system::init(system* esys, timing_system *timing_sys, manager_system *mngrsys,
        memory_system *mem_sys, io_system *io_sys, hle::lib_manager *lib_sys, arm::jit_interface *cpu) {
        // Intialize the uid with zero
        crr_uid.store(0);
        timing = timing_sys;
        mngr = mngrsys;
        mem = mem_sys;
        libmngr = lib_sys;
        io = io_sys;
        sys = esys;

        thr_sch = std::make_shared<kernel::thread_scheduler>(timing, *cpu);
    }

    void kernel_system::shutdown() {
        thr_sch.reset();
        crr_uid.store(0);

        close_all_processes();

        for (auto& thr : threads) {
            close_thread(thr.second->unique_id());
        }
    }

    kernel::uid kernel_system::next_uid() {
        crr_uid++;
        return crr_uid.load();
    }

    bool kernel_system::run_thread(kernel::uid thr_id) {
        auto res = threads.find(thr_id);

        if (res == threads.end()) {
            return false;
        }

        thr_sch->schedule(res->second);

        return true;
    }

    thread_ptr kernel_system::crr_thread() {
        return thr_sch->current_thread();
    }

    process *kernel_system::spawn_new_process(std::string &path, std::string name, uint32_t uid) {
        std::u16string path16 = std::u16string(path.begin(), path.end());
        symfile f = io->open_file(path16, READ_MODE | BIN_MODE);

		if (!f) {
            return nullptr;
		}

        auto temp = loader::parse_eka2img(f, true);

		if (!temp) {
            return false;
        } else {
            set_epoc_version(temp->epoc_ver);

			// Lib manager needs the system to call HLE function
            libmngr->init(sys, this, io, mem, kern_ver);
		}

		auto res2 = libmngr->load_e32img(path16);

        if (!res2) {
            return nullptr;
        }

        crr_process_id = uid;
		libmngr->open_e32img(res2);

        processes.insert(std::make_pair(uid, std::make_shared<process>(this, mem, uid, name, path16, u"", res2)));

        LOG_INFO("Process name: {}, uid: 0x{:x} loaded, ready for command to run.", name, uid);

        crr_process_id = uid;

        return &(*processes[uid]);
    }

    process *kernel_system::spawn_new_process(uint32_t uid) {
        return spawn_new_process(mngr->get_package_manager()->get_app_executable_path(uid),
            mngr->get_package_manager()->get_app_name(uid), uid);
    }

    bool kernel_system::close_process(process *pr) {
        auto res = processes.find(pr->get_uid());

        if (res == processes.end()) {
            return false;
        }

        LOG_TRACE("Shutdown process with UID: 0x{:x}", pr->get_uid());

        pr->stop();

        libmngr->close_e32img(pr->get_e32img());
        processes.erase(pr->get_uid());

        if (processes.size() > 0)
            crr_process_id = processes.begin()->first;
        else
            crr_process_id = 0;

        return true;
    }

	bool kernel_system::close_process(const kernel::uid id) {
        auto res = processes.find(id);

        if (res == processes.end()) {
            return false;
        }

		process_ptr pr = res->second;

        LOG_TRACE("Shutdown process with UID: 0x{:x}", id);

        pr->stop();

        libmngr->close_e32img(pr->get_e32img());
        processes.erase(id);

        if (processes.size() > 0)
            crr_process_id = processes.begin()->first;
        else
            crr_process_id = 0;

        return true;
	}
    // TODO: Fix this poorly written code
    bool kernel_system::close_all_processes() {
        while (processes.size() > 0) {
            if (!close_process(&(*processes.begin()->second))) {
                return false;
            }
        }

        return true;
    }

    kernel::uid kernel_system::get_id_base_owner(kernel::owner_type owner) const {
        return owner == kernel::owner_type::process ? crr_process_id : thr_sch->current_thread()->unique_id();
    }

    chunk_ptr kernel_system::create_chunk(std::string name, const address bottom, const address top, const size_t size, prot protection,
        kernel::chunk_type type, kernel::chunk_access access, kernel::chunk_attrib attrib, kernel::owner_type owner, int64_t owner_id) {
        chunk_ptr new_chunk = std::make_shared<kernel::chunk>(this, mem, name, bottom, top, size, protection, type, access, attrib, 
            owner, owner_id < 0 ? get_id_base_owner(owner) : static_cast<kernel::uid>(owner_id));
        uint32_t id = new_chunk->unique_id();

        chunks.emplace(new_chunk->unique_id(), std::move(new_chunk));

        return chunks[id];
    }

    mutex_ptr kernel_system::create_mutex(std::string name, bool init_locked,
        kernel::owner_type own,
        kernel::uid own_id,
        kernel::access_type access) {
        mutex_ptr new_mutex = std::make_shared<kernel::mutex>(this, name, init_locked, own, 
            own_id < 0 ? get_id_base_owner(own) : static_cast<kernel::uid>(own_id), access);

        uint32_t id = new_mutex->unique_id();

        mutexes.emplace(new_mutex->unique_id(), std::move(new_mutex));

        return mutexes[id];
    }

    sema_ptr kernel_system::create_sema(std::string sema_name,
        int32_t init_count,
        int32_t max_count,
        kernel::owner_type own_type,
        kernel::uid own_id,
        kernel::access_type access) {
        sema_ptr new_sema = std::make_shared<kernel::semaphore>(this, sema_name, init_count, max_count, own_type,
            own_id < 0 ? get_id_base_owner(own_type) : static_cast<kernel::uid>(own_id), access);

        uint32_t id = new_sema->unique_id();

        semas.emplace(new_sema->unique_id(), std::move(new_sema));

        return semas[id];
    }

    timer_ptr kernel_system::create_timer(std::string name, kernel::reset_type rt,
        kernel::owner_type owner,
        kernel::uid own_id,
        kernel::access_type access) {
        timer_ptr new_timer = std::make_shared<kernel::timer>(this, timing, name, rt, owner,
            own_id < 0 ? get_id_base_owner(owner) : static_cast<kernel::uid>(own_id), access);

        uint32_t id = new_timer->unique_id();

        timers.emplace(new_timer->unique_id(), std::move(new_timer));

        return timers[id];
    }

    bool kernel_system::close_timer(kernel::uid id) {
        auto res = timers.find(id);

        if (res != timers.end()) {
            timers.erase(id);
            return true;
        }

        return false;
    }

    bool kernel_system::close_sema(kernel::uid id) {
        auto res = semas.find(id);

        if (res != semas.end()) {
            semas.erase(id);
            return true;
        }

        return false;
    }

    bool kernel_system::close_mutex(kernel::uid id) {
        auto res = mutexes.find(id);

        if (res != mutexes.end()) {
            mutexes.erase(id);
            return true;
        }

        return false;
    }

    bool kernel_system::close_chunk(kernel::uid id) {
        auto res = chunks.find(id);

        if (res != chunks.end()) {
            chunks[id]->destroy();
            chunks.erase(res);
            return true;
        }

        return false;
    }

    bool kernel_system::close_thread(kernel::uid id) {
        auto res = threads.find(id);

        if (res != threads.end()) {
            threads.erase(id);

            for (auto it = chunks.begin(); it != chunks.end(); ) {
                // Erase all chunks that have relation to the thread
                if (it->second && it->second->obj_owner() == id) {
                    uint32_t id = (it++)->first;
                    close_chunk(id);
                } else {
                    ++it;
                }
            }

            return true;
        }

        return false;
    }

    bool kernel_system::close(kernel::uid id) {
        // TODO: Remove this horrible if
        if (!close_chunk(id)) {
            if (!close_thread(id)) {
                if (!close_mutex(id)) {
                    if (!close_sema(id)) {
                        if (!close_timer(id)) {
                            return false;
                        }
                    }
                }
            }
        }

        return true;
    }

    thread_ptr kernel_system::add_thread(kernel::owner_type owner, kernel::uid owner_id, kernel::access_type access,
        const std::string &name, const address epa, const size_t stack_size,
        const size_t min_heap_size, const size_t max_heap_size,
        ptr<void> usrdata,
        kernel::thread_priority pri) {
        thread_ptr new_thread = std::make_shared<kernel::thread>(this, mem, owner, owner_id, access, name, epa, stack_size, min_heap_size, max_heap_size, usrdata, pri);
        uint32_t id = new_thread->unique_id();

        thr_sch->schedule(new_thread);
        threads.emplace(new_thread->unique_id(), std::move(new_thread));
        
        return threads[id];
    }

    void kernel_system::set_closeable(kernel::uid id, bool opt) {
        kernel_obj_ptr obj = get_kernel_obj(id);

        if (obj == nullptr) {
            LOG_WARN("Get closeable attribute of unexist kernel object");
            return;
        }

        obj->user_closeable(opt);
    }

    bool kernel_system::get_closeable(kernel::uid id) {
        kernel_obj_ptr obj = get_kernel_obj(id);

        if (!obj) {
            return false;
        }

        return obj->user_closeable();
    }

    kernel_obj_ptr kernel_system::get_kernel_obj(kernel::uid id) {
        auto chunk_ite = chunks.find(id);

        if (chunk_ite != chunks.end()) {
            return chunk_ite->second;
        } 

        auto thread_ite = threads.find(id);

        if (thread_ite != threads.end()) {
            return thread_ite->second;
        }

        auto timer_ite = timers.find(id);

        if (timer_ite != timers.end()) {
            return timer_ite->second;
        }

        auto sema_ite = semas.find(id);

        if (sema_ite != semas.end()) {
            return sema_ite->second;
        }

        auto mutex_ite = mutexes.find(id);

        if (mutex_ite != mutexes.end()) {
            return mutex_ite->second;
        }

        return nullptr;
    }

    thread_ptr kernel_system::get_thread_by_name(const std::string &name) {
        auto thr_pair_find = std::find_if(threads.begin(), threads.end(),
            [&](auto &thr_pair) { return thr_pair.second->name() == name; });

        if (thr_pair_find == threads.end()) {
            return nullptr;
        }

        return thr_pair_find->second;
    }
}

