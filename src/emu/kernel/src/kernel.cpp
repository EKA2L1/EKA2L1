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

#include <cpu/arm_analyser.h>
#include <cpu/arm_interface.h>
#include <cpu/arm_utils.h>

#include <common/buffer.h>
#include <common/chunkyseri.h>
#include <common/configure.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <common/virtualmem.h>

#include <disasm/disasm.h>

#include <kernel/kernel.h>
#include <kernel/libmanager.h>
#include <kernel/scheduler.h>
#include <kernel/thread.h>
#include <loader/romimage.h>
#include <mem/mem.h>
#include <mem/ptr.h>
#include <vfs/vfs.h>

#include <loader/e32img.h>
#include <loader/romimage.h>

#include <common/time.h>
#include <config/config.h>

namespace eka2l1 {
    void kernel_global_data::reset() {
        // Reset all these to 0
        char_set_.char_data_set_ = 0;
        char_set_.collation_data_set_ = 0;
    }

    kernel_system::kernel_system(system *esys, ntimer *timing, io_system *io_sys,
        config::state *old_conf, config::app_settings *settings, loader::rom *rom_info, arm::core *cpu, disasm *disassembler)
        : btrace_inst_(nullptr)
        , lib_mngr_(nullptr)
        , thr_sch_(nullptr)
        , timing_(timing)
        , io_(io_sys)
        , sys_(esys)
        , conf_(old_conf)
        , app_settings_(settings)
        , disassembler_(disassembler)
        , cpu_(cpu)
        , rom_info_(rom_info)
        , kernel_handles_(this, kernel::handle_array_owner::kernel)
        , realtime_ipc_signal_evt_(0)
        , uid_counter_(0)
        , rom_map_(nullptr)
        , kern_ver_(epocver::epoc94)
        , lang_(language::en)
        , global_data_chunk_(nullptr)
        , dll_global_data_chunk_(nullptr)
        , dll_global_data_last_offset_(0)
        , inactivity_starts_(0) {
        thr_sch_ = std::make_unique<kernel::thread_scheduler>(this, timing_, cpu_);

        // Instantiate btrace
        btrace_inst_ = std::make_unique<kernel::btrace>(this, io_);

        // Create real time IPC event
        realtime_ipc_signal_evt_ = timing->register_event("RealTimeIpc", [this](std::uint64_t userdata, std::uint64_t cycles_late) {
            kernel::thread *thr = get_by_id<kernel::thread>(static_cast<kernel::uid>(userdata));
            assert(thr);

            lock();
            thr->signal_request();
            unlock();
        });

        // Get base time
        base_time_ = common::get_current_time_in_microseconds_since_1ad();
        locale_ = std::make_unique<std::locale>("");
    }

    kernel_system::~kernel_system() {
        reset();
    }

    void kernel_system::reset() {
        if (rom_map_) {
            common::unmap_file(rom_map_);
        }

        rom_map_ = nullptr;

#define OBJECT_CONTAINER_CLEANUP(container)             \
    for (auto &obj: container) {                        \
        obj->destroy();                                 \
    }                                                   \
    container.clear();

        // Delete one by one in order. Do not change the order
        OBJECT_CONTAINER_CLEANUP(sessions_);
        OBJECT_CONTAINER_CLEANUP(servers_);
        OBJECT_CONTAINER_CLEANUP(timers_);
        OBJECT_CONTAINER_CLEANUP(mutexes_);
        OBJECT_CONTAINER_CLEANUP(semas_);
        OBJECT_CONTAINER_CLEANUP(change_notifiers_);
        OBJECT_CONTAINER_CLEANUP(props_);
        OBJECT_CONTAINER_CLEANUP(prop_refs_);
        OBJECT_CONTAINER_CLEANUP(chunks_);
        OBJECT_CONTAINER_CLEANUP(threads_);
        OBJECT_CONTAINER_CLEANUP(processes_);
        OBJECT_CONTAINER_CLEANUP(libraries_);
        OBJECT_CONTAINER_CLEANUP(codesegs_);
        OBJECT_CONTAINER_CLEANUP(message_queues_);

        btrace_inst_->close_trace_session();
    }

    void kernel_system::cpu_exception_thread_handle(arm::core *core) {
        std::uint8_t *pc_data = reinterpret_cast<std::uint8_t *>(crr_process()->get_ptr_on_addr_space(core->get_pc()));

        if (pc_data) {
            const std::string disassemble_inst = disassembler_->disassemble(pc_data,
                (core->get_cpsr() & 0x20) ? 2 : 4, core->get_pc(),
                (core->get_cpsr() & 0x20) ? true : false);

            LOG_TRACE("Last instruction: {} (0x{:x})", disassemble_inst, (core->get_cpsr() & 0x20)
                    ? *reinterpret_cast<std::uint16_t *>(pc_data)
                    : *reinterpret_cast<std::uint32_t *>(pc_data));
        }

        pc_data = reinterpret_cast<std::uint8_t *>(crr_process()->get_ptr_on_addr_space(core->get_lr()));

        if (pc_data) {
            const std::string disassemble_inst = disassembler_->disassemble(pc_data,
                core->get_lr() % 2 != 0 ? 2 : 4, core->get_lr() - core->get_lr() % 2,
                core->get_lr() % 2 != 0 ? true : false);

            LOG_TRACE("LR instruction: {} (0x{:x})", disassemble_inst, (core->get_lr() % 2 != 0)
                    ? *reinterpret_cast<std::uint16_t *>(pc_data)
                    : *reinterpret_cast<std::uint32_t *>(pc_data));
        }

        kernel::thread *target_to_stop = crr_thread();

        core->stop();

        target_to_stop->stop();
        core->save_context(target_to_stop->get_thread_context());

        // Dump thread contexts
        arm::dump_context(target_to_stop->get_thread_context());
    }

    bool kernel_system::cpu_exception_handle_unpredictable(arm::core *core, const address occurred) {
        auto read_crr_func = [&](const address addr) -> std::uint32_t {
            const std::uint32_t *val = reinterpret_cast<std::uint32_t*>(crr_process()->get_ptr_on_addr_space(addr));
            return val ? *val : 0;
        };

        if (!analyser_) {
            analyser_ = arm::make_analyser(arm::arm_disassembler_backend::capstone, read_crr_func);
        }

        std::uint32_t inst_value = read_crr_func(occurred & ~1);
        if (occurred & 1) {
            // Take only the thumb part
            inst_value >>= 16;
        }

        // Find entry in cache
        auto result_find = cache_inters_.find(inst_value);
        if (result_find != cache_inters_.end()) {
            result_find->second(core);
            return true;
        }

        auto inst = analyser_->next_instruction(occurred);
        if (!inst) {
            return false;
        }

        switch (inst->iname) {
        case arm::instruction::MOV: {
            if ((inst->ops.size() != 2) || (inst->ops[0].type != arm::arm_op_type::op_reg) || (inst->ops[1].type != arm::arm_op_type::op_reg)) {
                return false;
            }

            const std::uint32_t source_reg = static_cast<int>(inst->ops[1].reg) - static_cast<int>(arm::reg::R0);
            const std::uint32_t dest_reg = static_cast<int>(inst->ops[0].reg) - static_cast<int>(arm::reg::R0);
        
            auto func_generated = [=](arm::core *core) {
                auto source_val = core->get_reg(source_reg);

                if (dest_reg == 15) {
                    core->set_cpsr((source_val & 1) ? (core->get_cpsr() | 0x20) :
                        (core->get_cpsr() & ~0x20));
                }

                core->set_reg(dest_reg, source_val);
            };

            func_generated(core);

            cache_inters_.emplace(inst_value, func_generated);
            break;
        }

        default:
            return false;
        }

        return true;
    }

    void kernel_system::cpu_exception_handler(arm::core *core, arm::exception_type exception_type, const std::uint32_t exception_data) {
        switch (exception_type) {
        case arm::exception_type_access_violation_read:
        case arm::exception_type_access_violation_write:
            LOG_ERROR("Access violation {} address 0x{:X} in thread {}", (exception_type == arm::exception_type_access_violation_read) ?
                      "reading" : "writing", exception_data, crr_thread()->name());
            break;

        case arm::exception_type_undefined_inst:
            LOG_ERROR("Undefined instruction encountered in thread {}", crr_thread()->name());
            break;

        case arm::exception_type_unpredictable:
            if (!cpu_exception_handle_unpredictable(core, exception_data)) {
                break;
            }

            return;

        case arm::exception_type_breakpoint:
            for (auto &breakpoint_callback_func: breakpoint_callbacks_) {
                breakpoint_callback_func(core, crr_thread(), exception_data);
            }

            return;

        default:
            LOG_ERROR("Unknown exception encountered in thread {}", crr_thread()->name());
            break;
        }

        cpu_exception_thread_handle(core);
    }

    void kernel_system::install_memory(memory_system *new_mem) {
        mem_ = new_mem;
    }
    
    // For user-provided EPOC version
    void kernel_system::set_epoc_version(const epocver ver) {
        kern_ver_ = ver;
        lib_mngr_ = std::make_unique<hle::lib_manager>(this, io_, mem_);

        // Set CPU SVC handler
        cpu_->system_call_handler = [this](const std::uint32_t ordinal) {
            get_lib_manager()->call_svc(ordinal);

            // EKA1 does not use BX LR to jump back, they let kernel do it
            if (is_eka1()) {
                const std::uint32_t jump_back = cpu_->get_lr();

                // Set pc and ARM/thumb flag
                cpu_->set_pc(jump_back & ~0b1);
                cpu_->set_cpsr(cpu_->get_cpsr() | ((jump_back & 0b1) ? 0x20 : 0));
            }
        };

        cpu_->exception_handler = [this](arm::exception_type exception_type, const std::uint32_t data) {
            cpu_exception_handler(cpu_, exception_type, data);
        };
    }

    eka2l1::ptr<kernel_global_data> kernel_system::get_global_user_data_pointer() {
        if (!global_data_chunk_) {    
            // Make global data
            static constexpr std::uint32_t GLOBAL_DATA_SIZE = 0x1000;
            global_data_chunk_ = create<kernel::chunk>(mem_, nullptr, "Kernel global data", 0, GLOBAL_DATA_SIZE,
                GLOBAL_DATA_SIZE, prot::read_write, kernel::chunk_type::normal, kernel::chunk_access::rom,
                kernel::chunk_attrib::none, 0x00);

            if (global_data_chunk_) {
                kernel_global_data *data = reinterpret_cast<kernel_global_data*>(global_data_chunk_->host_base());
                data->reset();
            }
        }

        return global_data_chunk_->base(nullptr).cast<kernel_global_data>();
    }
    
    kernel::thread *kernel_system::crr_thread() {
        return thr_sch_->current_thread();
    }

    kernel::process *kernel_system::crr_process() {
        return thr_sch_->current_process();
    }

    arm::core *kernel_system::get_cpu() {
        return cpu_;
    }

    void kernel_system::reschedule() {
        lock();
        thr_sch_->reschedule();
        unlock();
    }

    void kernel_system::unschedule_wakeup() {
        thr_sch_->unschedule_wakeup();
    }
    
    void kernel_system::prepare_reschedule() {
        get_cpu()->stop();
    }

    void kernel_system::call_ipc_send_callbacks(const std::string &server_name, const int ord, const ipc_arg &args,
        kernel::thread *callee) {
        for (auto &ipc_send_callback_func: ipc_send_callbacks_) {
            ipc_send_callback_func(server_name, ord, args, callee);
        }
    }

    void kernel_system::call_ipc_complete_callbacks(ipc_msg *msg, const int complete_code) {
        for (auto &ipc_complete_callback_func: ipc_complete_callbacks_) {
            ipc_complete_callback_func(msg, complete_code);
        }
    }

    void kernel_system::call_thread_kill_callbacks(kernel::thread *target, const std::string &category, const std::int32_t reason) {
        for (auto &thread_kill_callback_func: thread_kill_callbacks_) {
            thread_kill_callback_func(target, category, reason);
        }
    }

    void kernel_system::call_process_switch_callbacks(arm::core *run_core, kernel::process *old, kernel::process *new_one) {
        for (auto &process_switch_callback_func: process_switch_callback_funcs_) {
            process_switch_callback_func(run_core, old, new_one);
        }
    }

    void kernel_system::run_codeseg_loaded_callback(const std::string &lib_name, kernel::process *attacher, codeseg_ptr target) {
        for (auto &codeseg_loaded_callback_func: codeseg_loaded_callback_funcs_) {
            codeseg_loaded_callback_func(lib_name, attacher, target);
        }
    }

    void kernel_system::run_imb_range_callback(kernel::process *caller, address range_addr, const std::size_t range_size) {
        for (auto &imb_range_callback_func: imb_range_callback_funcs_) {
            imb_range_callback_func(caller, range_addr, range_size);
        }
    }

    std::size_t kernel_system::register_process_switch_callback(process_switch_callback callback) {
        return process_switch_callback_funcs_.add(callback);
    }

    std::size_t kernel_system::register_ipc_send_callback(ipc_send_callback callback) {
        return ipc_send_callbacks_.add(callback);
    }
    
    std::size_t kernel_system::register_ipc_complete_callback(ipc_complete_callback callback) {
        return ipc_complete_callbacks_.add(callback);
    }

    std::size_t kernel_system::register_thread_kill_callback(thread_kill_callback callback) {
        return thread_kill_callbacks_.add(callback);
    }

    std::size_t kernel_system::register_breakpoint_hit_callback(breakpoint_callback callback) {
        return breakpoint_callbacks_.add(callback);
    }

    std::size_t kernel_system::register_codeseg_loaded_callback(codeseg_loaded_callback callback) {
        return codeseg_loaded_callback_funcs_.add(callback);
    }

    std::size_t kernel_system::register_imb_range_callback(imb_range_callback callback) {
        return imb_range_callback_funcs_.add(callback);
    }
    
    bool kernel_system::unregister_ipc_send_callback(const std::size_t handle) {
        return ipc_send_callbacks_.remove(handle);
    }

    bool kernel_system::unregister_ipc_complete_callback(const std::size_t handle) {
        return ipc_complete_callbacks_.remove(handle);
    }

    bool kernel_system::unregister_thread_kill_callback(const std::size_t handle) {
        return thread_kill_callbacks_.remove(handle);
    }
    
    bool kernel_system::unregister_breakpoint_hit_callback(const std::size_t handle) {
        return breakpoint_callbacks_.remove(handle);
    }
    
    bool kernel_system::unregister_process_switch_callback(const std::size_t handle) {
        return process_switch_callback_funcs_.remove(handle);
    }

    bool kernel_system::unregister_codeseg_loaded_callback(const std::size_t handle) {
        return codeseg_loaded_callback_funcs_.remove(handle);
    }

    bool kernel_system::unregister_imb_range_callback(const std::size_t handle) {
        return imb_range_callback_funcs_.remove(handle);
    }

    ipc_msg_ptr kernel_system::create_msg(kernel::owner_type owner) {
        auto slot_free = std::find_if(msgs_.begin(), msgs_.end(),
            [](auto slot) { return !slot || slot->free; });

        if (slot_free != msgs_.end()) {
            if (!*slot_free) {
                ipc_msg_ptr msg = std::make_shared<ipc_msg>(crr_thread());
                *slot_free = std::move(msg);
            }

            slot_free->get()->own_thr = crr_thread();
            slot_free->get()->free = false;
            slot_free->get()->id = static_cast<std::uint32_t>(slot_free - msgs_.begin());

            return *slot_free;
        }

        return nullptr;
    }

    ipc_msg_ptr kernel_system::get_msg(int handle) {
        if (msgs_.size() <= handle) {
            return nullptr;
        }

        return msgs_[handle];
    }

    bool kernel_system::destroy(kernel_obj_ptr obj) {
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

            OBJECT_SEARCH(mutex, mutexes_)
            OBJECT_SEARCH(sema, semas_)
            OBJECT_SEARCH(chunk, chunks_)
            OBJECT_SEARCH(thread, threads_)
            OBJECT_SEARCH(process, processes_)
            OBJECT_SEARCH(change_notifier, change_notifiers_)
            OBJECT_SEARCH(library, libraries_)
            OBJECT_SEARCH(codeseg, codesegs_)
            OBJECT_SEARCH(server, servers_)
            OBJECT_SEARCH(prop, props_)
            OBJECT_SEARCH(prop_ref, prop_refs_)
            OBJECT_SEARCH(session, sessions_)
            OBJECT_SEARCH(timer, timers_)
            OBJECT_SEARCH(msg_queue, message_queues_)

#undef OBJECT_SEARCH

        default:
            break;
        }

        return false;
    }

    // We can support also ELF!
    process_ptr kernel_system::spawn_new_process(const std::u16string &path, const std::u16string &cmd_arg, const kernel::uid promised_uid3,
        const std::uint32_t stack_size) {
        std::u16string full_path;
        auto imgs = lib_mngr_->try_search_and_parse(path, &full_path);

        if (!imgs.first && !imgs.second) {
            return nullptr;
        }

        std::string path8 = common::ucs2_to_utf8(path);
        std::string process_name = eka2l1::filename(path8);

        /* Create process through kernel system. */
        process_ptr pr = create<kernel::process>(mem_, process_name, path, cmd_arg);

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
                new_stack_size = std::max<std::uint32_t>(imgs.first->header.stack_size, stack_size);
            }

            pri = static_cast<kernel::process_priority>(imgs.first->header.priority);
            heap_min = imgs.first->header.heap_size_min;
            heap_max = imgs.first->header.heap_size_max;

            cs = lib_mngr_->load_as_e32img(*eimg, full_path);
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

            cs = lib_mngr_->load_as_romimg(*imgs.second, full_path);
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
            return kernel_handles_.close(handle);
        }

        return crr_process()->process_handles.close(handle);
    }

    kernel_obj_ptr kernel_system::get_kernel_obj_raw(uint32_t handle) {
        if ((handle & ~0x8000) == 0xFFFF0000) {
            return reinterpret_cast<kernel::kernel_obj *>(get_by_id<kernel::process>(
                crr_process()->unique_id()));
        } else if ((handle & ~0x8000) == 0xFFFF0001) {
            return reinterpret_cast<kernel::kernel_obj *>(get_by_id<kernel::thread>(
                crr_thread()->unique_id()));
        }

        kernel::handle_inspect_info info = kernel::inspect_handle(handle);

        if (info.handle_array_local) {
            return crr_thread()->thread_handles.get_object(handle);
        }

        if (info.handle_array_kernel) {
            return kernel_handles_.get_object(handle);
        }

        return crr_process()->process_handles.get_object(handle);
    }

    bool kernel_system::get_info(kernel_obj_ptr the_object, kernel::handle_info &info) {
        if (!the_object) {
            return false;
        }

        kernel::thread *current_thread = crr_thread();
        kernel::process *current_process = crr_process();

        info.num_open_in_current_thread_ = current_thread->thread_handles.count(the_object);
        info.num_open_in_current_process_ = current_process->process_handles.count(the_object);
        info.num_threads_using_ = 0;
        info.num_processes_using_ = 0;

        for (std::size_t i = 0; i < threads_.size(); i++) {
            kernel::thread *thr = reinterpret_cast<kernel::thread*>(threads_[i].get());

            if (thr->thread_handles.has(the_object)) {
                info.num_threads_using_++;
            }

            if (thr->owning_process() == current_process) {
                info.num_open_in_current_process_++;
            }
        }

        for (std::size_t i = 0; i < processes_.size(); i++) {
            kernel::process *pr = reinterpret_cast<kernel::process*>(processes_[i].get());

            if (pr->process_handles.has(the_object)) {
                info.num_processes_using_++;
            }
        }

        return true;
    }

    void kernel_system::free_msg(ipc_msg_ptr msg) {
        if (msg->locked()) {
            return;
        }

        msg->free = true;
    }

    /*! \brief Completely destroy a message. */
    void kernel_system::destroy_msg(ipc_msg_ptr msg) {
        (msgs_.begin() + msg->id)->reset();
    }

    property_ptr kernel_system::get_prop(int category, int key) {
        auto prop_res = std::find_if(props_.begin(), props_.end(),
            [=](const auto &prop_obj) {
                property_ptr prop = reinterpret_cast<property_ptr>(prop_obj.get());
                if (prop->first == category && prop->second == key) {
                    return true;
                }

                return false;
            });

        if (prop_res == props_.end()) {
            return property_ptr(nullptr);
        }

        return reinterpret_cast<property_ptr>(prop_res->get());
    }

    property_ptr kernel_system::delete_prop(int category, int key) {
        auto prop_res = std::find_if(props_.begin(), props_.end(),
            [=](const auto &prop_obj) {
                property_ptr prop = reinterpret_cast<property_ptr>(prop_obj.get());
                if (prop->first == category && prop->second == key) {
                    return true;
                }

                return false;
            });

        if (prop_res == props_.end()) {
            return property_ptr(nullptr);
        }

        property_ptr prop_ptr = reinterpret_cast<property_ptr>(prop_res->get());
        props_.erase(prop_res);

        return prop_ptr;
    }

    kernel::handle kernel_system::mirror(kernel::thread *own_thread, kernel::handle handle, kernel::owner_type owner) {
        kernel_obj_ptr target_obj = get_kernel_obj_raw(handle);
        kernel::handle_inspect_info info = kernel::inspect_handle(handle);

        kernel::handle h = kernel::INVALID_HANDLE;

        switch (owner) {
        case kernel::owner_type::kernel:
            return kernel_handles_.add_object(target_obj);

        case kernel::owner_type::thread: {
            if (!own_thread) {
                return kernel::INVALID_HANDLE;
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

        if (h != kernel::INVALID_HANDLE) {
            target_obj->open_to(own_thread->owning_process());
        }

        return h;
    }

    uint32_t kernel_system::mirror(kernel_obj_ptr obj, kernel::owner_type owner) {
        kernel::handle h = kernel::INVALID_HANDLE;

        switch (owner) {
        case kernel::owner_type::kernel:
            return kernel_handles_.add_object(obj);

        case kernel::owner_type::thread: {
            h = crr_thread()->thread_handles.add_object(obj);
            break;
        }

        case kernel::owner_type::process: {
            h = crr_process()->process_handles.add_object(obj);
            break;
        }

        default:
            return kernel::INVALID_HANDLE;
        }

        if (h != kernel::INVALID_HANDLE) {
            obj->open_to(crr_process());
        }

        return h;
    }

    kernel::handle kernel_system::open_handle(kernel_obj_ptr obj, kernel::owner_type owner) {
        return open_handle_with_thread(crr_thread(), obj, owner);
    }

    kernel::handle kernel_system::open_handle_with_thread(kernel::thread *thr, kernel_obj_ptr obj, kernel::owner_type owner) {
        kernel::handle h = kernel::INVALID_HANDLE;

        switch (owner) {
        case kernel::owner_type::kernel:
            return kernel_handles_.add_object(obj);

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

        if (h != kernel::INVALID_HANDLE) {
            obj->open_to(thr->owning_process());
        }

        return h;
    }

    kernel::uid kernel_system::next_uid() const {
        ++uid_counter_;
        return uid_counter_.load();
    }

    void kernel_system::setup_new_process(process_ptr pr) {
        // TODO(pent0): Hope i can enable this server again, it's currently buggy
        // std::unique_ptr<service::server> ps_srv = std::make_unique<eka2l1::posix_server>(sys, pr);
        // add_custom_server(ps_srv);
    }

    codeseg_ptr kernel_system::pull_codeseg_by_ep(const address ep) {
        auto res = std::find_if(codesegs_.begin(), codesegs_.end(), [=](const auto &cs) -> bool {
            return reinterpret_cast<codeseg_ptr>(cs.get())->get_entry_point(nullptr) == ep;
        });

        if (res == codesegs_.end()) {
            return nullptr;
        }

        return reinterpret_cast<codeseg_ptr>(res->get());
    }

    codeseg_ptr kernel_system::pull_codeseg_by_uids(const kernel::uid uid0, const kernel::uid uid1,
        const kernel::uid uid2) {
        auto res = std::find_if(codesegs_.begin(), codesegs_.end(), [=](const auto &cs) -> bool {
            return reinterpret_cast<codeseg_ptr>(cs.get())->get_uids() == std::make_tuple(uid0, uid1, uid2);
        });

        if (res == codesegs_.end()) {
            return nullptr;
        }

        return reinterpret_cast<codeseg_ptr>(res->get());
    }

    std::optional<find_handle> kernel_system::find_object(const std::string &name, int start, kernel::object_type type, const bool use_full_name) {
        find_handle handle_find_info;
        std::regex filter(common::wildcard_to_regex_string(name));

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
            return std::regex_match(to_compare, filter);                                       \
        });                                                                                    \
        if (res == obj_map.end())                                                              \
            return std::nullopt;                                                               \
        handle_find_info.index = static_cast<int>(std::distance(obj_map.begin(), res));        \
        handle_find_info.object_id = (*res)->unique_id();                                      \
        handle_find_info.obj = reinterpret_cast<kernel_obj_ptr>(res->get());                   \
        return handle_find_info;                                                               \
    }

            OBJECT_SEARCH(mutex, mutexes_)
            OBJECT_SEARCH(sema, semas_)
            OBJECT_SEARCH(chunk, chunks_)
            OBJECT_SEARCH(thread, threads_)
            OBJECT_SEARCH(process, processes_)
            OBJECT_SEARCH(change_notifier, change_notifiers_)
            OBJECT_SEARCH(library, libraries_)
            OBJECT_SEARCH(codeseg, codesegs_)
            OBJECT_SEARCH(server, servers_)
            OBJECT_SEARCH(prop, props_)
            OBJECT_SEARCH(prop_ref, prop_refs_)
            OBJECT_SEARCH(session, sessions_)
            OBJECT_SEARCH(timer, timers_)
            OBJECT_SEARCH(msg_queue, message_queues_)

#undef OBJECT_SEARCH

        default:
            break;
        }

        return std::nullopt;
    }

    bool kernel_system::should_terminate() {
        return thr_sch_->should_terminate();
    }

    bool kernel_system::map_rom(const mem::vm_address addr, const std::string &path) {
        rom_map_ = common::map_file(path, prot::read_write, 0, true);
        const std::size_t rom_size = common::file_size(path);

        if (!rom_map_) {
            return false;
        }

        LOG_TRACE("Rom mapped to address: 0x{:x}", reinterpret_cast<std::uint64_t>(rom_map_));

        // Don't care about the result as long as it's not null.
        kernel::chunk *rom_chunk = create<kernel::chunk>(mem_, nullptr, "ROM", 0, static_cast<address>(rom_size),
            rom_size, prot::read_write_exec, kernel::chunk_type::normal, kernel::chunk_access::rom,
            kernel::chunk_attrib::none, 0x00, false, addr, rom_map_);

        if (!rom_chunk) {
            LOG_ERROR("Can't create ROM chunk!");

            common::unmap_file(rom_map_);
            return false;
        }

        return true;
    }

    void kernel_system::stop_cores_idling() {
        if (should_core_idle_when_inactive()) {
            thr_sch_->stop_idling();
        }
    }

    bool kernel_system::should_core_idle_when_inactive() {
        return conf_->cpu_load_save;
    }

    void kernel_system::set_current_language(const language new_lang) {
        lang_ = new_lang;
    }

    address kernel_system::get_global_dll_space(const address handle, std::uint8_t **data_ptr, std::uint32_t *size_of_data) {
        if (!dll_global_data_chunk_) {
            return 0;
        }

        auto find_result = dll_global_data_offset_.find(handle);
        if (find_result == dll_global_data_offset_.end()) {
            return 0;
        }

        if (data_ptr) {
            *data_ptr = reinterpret_cast<std::uint8_t*>(dll_global_data_chunk_->host_base()) + static_cast<std::uint32_t>(find_result->second);
        }

        if (size_of_data) {
            *size_of_data = static_cast<std::uint32_t>(find_result->second >> 32);
        }

        return static_cast<std::uint32_t>(find_result->second) + dll_global_data_chunk_->base(nullptr).ptr_address();
    }
    
    bool kernel_system::allocate_global_dll_space(const address handle, const std::uint32_t size, 
        address &data_ptr_guest, std::uint8_t **data_ptr_host) {
        static constexpr std::uint32_t GLOBAL_DLL_DATA_CHUNK_SIZE = 0x100000;
        
        if (!dll_global_data_chunk_) {
            dll_global_data_chunk_ = create<kernel::chunk>(mem_, nullptr, "EKA1_DllGlobalDataChunk",
                0, GLOBAL_DLL_DATA_CHUNK_SIZE, static_cast<std::size_t>(GLOBAL_DLL_DATA_CHUNK_SIZE), prot::read_write,
                kernel::chunk_type::normal, kernel::chunk_access::global, kernel::chunk_attrib::none, 0);
        }

        auto find_result = dll_global_data_offset_.find(handle);
        if (find_result != dll_global_data_offset_.end()) {
            return false;
        }

        if (dll_global_data_last_offset_ + size > GLOBAL_DLL_DATA_CHUNK_SIZE) {
            return false;
        }

        dll_global_data_offset_.emplace(handle, (static_cast<std::uint64_t>(size) << 32) | dll_global_data_last_offset_);
        data_ptr_guest = dll_global_data_chunk_->base(nullptr).ptr_address() + dll_global_data_last_offset_;

        if (data_ptr_host) {
            *data_ptr_host = reinterpret_cast<std::uint8_t*>(dll_global_data_chunk_->host_base()) + dll_global_data_last_offset_;
        }
        
        dll_global_data_last_offset_ += size;
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
        return base_time_ + timing_->microseconds();
    }

    void kernel_system::set_base_time(std::uint64_t time) {
        base_time_ = time;
    }

    std::uint32_t kernel_system::inactivity_time() {
        if (!inactivity_starts_) {
            inactivity_starts_ = timing_->microseconds();
        }

        return static_cast<std::uint32_t>((timing_->microseconds() - inactivity_starts_) /
            common::microsecs_per_sec);
    }

    void kernel_system::reset_inactivity_time() {
        inactivity_starts_ = timing_->microseconds();
    }
}
