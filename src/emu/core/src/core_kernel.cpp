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
    void kernel_system::init(timing_system *sys, manager_system *mngrsys,
        memory_system *mem_sys, io_system *io_sys, hle::lib_manager *lib_sys, arm::jit_interface *cpu) {
        // Intialize the uid with zero
        crr_uid.store(0);
        timing = sys;
        mngr = mngrsys;
        mem = mem_sys;
        libmngr = lib_sys;
        io = io_sys;
        thr_sch = std::make_shared<kernel::thread_scheduler>(sys, *cpu);
    }

    void kernel_system::shutdown() {
        thr_sch.reset();
        crr_uid.store(0);

        close_all_processes();
    }

    kernel::uid kernel_system::next_uid() {
        crr_uid++;
        return crr_uid.load();
    }

    void kernel_system::add_thread(kernel::thread *thr) {
        const std::lock_guard<std::mutex> guard(mut);
        threads.emplace(thr->unique_id(), thr);
    }

    bool kernel_system::run_thread(kernel::uid thr_id) {
        auto res = threads.find(thr_id);

        if (res == threads.end()) {
            return false;
        }

        thr_sch->schedule(res->second);

        return true;
    }

    kernel::thread *kernel_system::crr_thread() {
        return thr_sch->current_thread();
    }

    process *kernel_system::spawn_new_process(std::string &path, std::string name, uint32_t uid) {
        auto res2 = libmngr->load_e32img(std::u16string(path.begin(), path.end()));

        if (!res2) {
            return nullptr;
        }

        libmngr->open_e32img(res2);

        processes.insert(std::make_pair(uid, std::make_shared<process>(this, mem, uid, name, res2)));

        LOG_INFO("Process name: {}, uid: 0x{:x} loaded, ready for command to run.", name, uid);

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
}
