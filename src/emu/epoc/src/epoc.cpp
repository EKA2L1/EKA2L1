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

#include <common/configure.h>
#include <epoc/epoc.h>
#include <kernel/process.h>

#include <common/algorithm.h>
#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <common/platform.h>
#include <common/random.h>

#include <disasm/disasm.h>

#include <loader/e32img.h>
#include <manager/rpkg.h>

#include <epoc/hal.h>
#include <utils/panic.h>

#ifdef ENABLE_SCRIPTING
#include <manager/script_manager.h>
#endif

#include <atomic>
#include <fstream>
#include <string>

#include <disasm/disasm.h>
#include <drivers/itc.h>
#include <gdbstub/gdbstub.h>

#include <kernel/kernel.h>
#include <mem/mem.h>
#include <mem/ptr.h>

#include <dispatch/dispatcher.h>
#include <kernel/libmanager.h>
#include <loader/rom.h>
#include <kernel/timing.h>
#include <services/init.h>
#include <vfs/vfs.h>

#include <cpu/arm_factory.h>
#include <cpu/arm_utils.h>

#include <config/config.h>
#include <manager/device_manager.h>
#include <manager/manager.h>

namespace eka2l1 {
    /* A system instance, where all the magic happens.
     * Represents the Symbian system. You can switch the system version dynamically.
    */
    class system_impl {
        //! Global lock mutex for system.
        std::mutex mut;

        //! The cpu
        arm::core_instance cpu;
        arm_emulator_type cpu_type;

        drivers::graphics_driver *gdriver;
        drivers::audio_driver *adriver;

        std::unique_ptr<memory_system> mem;
        std::unique_ptr<kernel_system> kern;
        std::unique_ptr<ntimer> timing;

        manager_system mngr;

        //! The IO system
        io_system io;

        //! Disassembly helper.
        disasm asmdis;

        gdbstub gdb_stub;

        dispatch::dispatcher dispatcher;

        debugger_base *debugger;

        loader::rom romf;

        config::state *conf;

        bool reschedule_pending;

        bool exit = false;
        bool paused = false;

        std::unordered_map<std::string, bool> bool_configs;
        std::unordered_map<uint32_t, hal_instance> hals;

        bool startup_inited = false;

        std::optional<filesystem_id> rom_fs_id;
        std::optional<filesystem_id> physical_fs_id;

        system *parent;
        std::size_t gdb_stub_breakpoint_callback_handle;

    public:
        explicit system_impl(system *parent, drivers::graphics_driver *graphics_driver, drivers::audio_driver *audio_driver,
            config::state *conf);

        ~system_impl(){};

        void set_graphics_driver(drivers::graphics_driver *graphics_driver);
        void set_audio_driver(drivers::audio_driver *audio_driver);

        void set_debugger(debugger_base *new_debugger) {
            debugger = new_debugger;
        }

        void set_symbian_version_use(const epocver ever) {
            io.set_epoc_ver(ever);

            // Use flexible model on 9.5 and onwards.
            mem = std::make_unique<memory_system>(cpu.get(), conf, (kern->get_epoc_version() >= epocver::epoc95) ? mem::mem_model_type::flexible
                : mem::mem_model_type::multiple, is_epocver_eka1(ever) ? true : false);

            // Install memory to the kernel, then set epoc version
            kern->install_memory(mem.get());
            kern->set_epoc_version(ever);
    
            service::init_services(parent);

            // Try to set system language
            set_system_language(static_cast<language>(conf->language));

            epoc::init_hal(parent);

            // Initialize HLE finally
            dispatcher.init(kern.get(), timing.get());
            
            if (!gdb_stub.is_server_enabled() && conf->enable_gdbstub) {
                gdb_stub.init(kern.get(), &io);
                gdb_stub.toggle_server(true);
            }
            
            if (gdb_stub.is_server_enabled()) {
                gdb_stub_breakpoint_callback_handle = kern->register_breakpoint_hit_callback([this](arm::core *cpu_core, kernel::thread *target,
                    const std::uint32_t addr) {
                    if (gdb_stub.is_connected()) {
                        cpu_core->stop();
                        cpu_core->set_pc(addr);

                        cpu_core->save_context(target->get_thread_context());

                        gdb_stub.break_exec();
                        gdb_stub.send_trap_gdb(target, 5);
                    }
                });
            }
        }

        bool set_device(const std::uint8_t idx) {
            manager::device_manager *dmngr = mngr.get_device_manager();
            bool result = dmngr->set_current(idx);

            if (!result) {
                return false;
            }

            manager::device *dvc = dmngr->get_current();
            io.set_product_code(dvc->firmware_code);

            set_symbian_version_use(dvc->ver);
            return true;
        }

        void validate_current_device() {
            io.validate_for_host();
        }

        void set_cpu_executor_type(const arm_emulator_type type) {
            cpu_type = type;
        }

        const arm_emulator_type get_cpu_executor_type() const {
            return cpu_type;
        }

        config::state *get_config() {
            return conf;
        }

        void set_config(config::state *confs) {
            conf = confs;
        }

        loader::rom *get_rom_info() {
            return &romf;
        }

        epocver get_symbian_version_use() const {
            return kern->get_epoc_version();
        }

        void prepare_reschedule() {
            cpu->prepare_rescheduling();
            reschedule_pending = true;
        }

        const language get_system_language() const {
            return kern->get_current_language();
        }

        void set_system_language(const language new_lang) {
            kern->set_current_language(new_lang);
        }

        void startup();
        bool load(const std::u16string &path, const std::u16string &cmd_arg);
        int loop();
        void shutdown();

        bool pause();
        bool unpause();

        manager_system *get_manager_system() {
            return &mngr;
        }

        memory_system *get_memory_system() {
            return mem.get();
        }

        kernel_system *get_kernel_system() {
            return kern.get();
        }

        hle::lib_manager *get_lib_manager() {
            return kern->get_lib_manager();
        }

        io_system *get_io_system() {
            return &io;
        }

        ntimer *get_ntimer() {
            return timing.get();
        }

        disasm *get_disasm() {
            return &asmdis;
        }

        gdbstub *get_gdb_stub() {
            return &gdb_stub;
        }

        drivers::graphics_driver *get_graphic_driver() {
            return gdriver;
        }

        drivers::audio_driver *get_audio_driver() {
            return adriver;
        }

        arm::core *get_cpu() {
            return cpu.get();
        }

        dispatch::dispatcher *get_dispatcher() {
            return &dispatcher;
        }

        void mount(drive_number drv, const drive_media media, std::string path,
            const std::uint32_t attrib = io_attrib_none);

        void reset();

        /*! \brief Install an Z drive repackage. 
         *
         * \returns True on success.
         */
        bool install_rpkg(const std::string &devices_rom_path, const std::string &path, std::string &firmware_code);
        void load_scripts();

        void do_state(common::chunkyseri &seri);

        /*! \brief Install a SIS/SISX. */
        bool install_package(std::u16string path, drive_number drv);
        bool load_rom(const std::string &path);

        void request_exit();
        bool should_exit() const {
            return exit;
        }

        void add_new_hal(uint32_t hal_category, hal_instance &hal_com);
        epoc::hal *get_hal(uint32_t category);
    };

    void system_impl::load_scripts() {
#ifdef ENABLE_SCRIPTING
        std::string cur_dir;
        get_current_directory(cur_dir);

        common::dir_iterator scripts_dir(".//scripts//");
        scripts_dir.detail = true;

        common::dir_entry scripts_entry;

        while (scripts_dir.next_entry(scripts_entry) == 0) {
            if ((scripts_entry.type == common::FILE_REGULAR) && path_extension(scripts_entry.name) == ".py") {
                auto module_name = replace_extension(filename(scripts_entry.name), "");
                mngr.get_script_manager()->import_module(".//scripts//" + module_name);
            }
        }

        set_current_directory(cur_dir);
#endif
    }

    void system_impl::do_state(common::chunkyseri &seri) {
    }

    static constexpr std::uint32_t DEFAULT_CPU_HZ = 484000000;

    void system_impl::startup() {
        exit = false;

        // Initialize all the system that doesn't depend on others first
        timing = std::make_unique<ntimer>(DEFAULT_CPU_HZ);
        asmdis.init();

        file_system_inst physical_fs = create_physical_filesystem(epocver::epoc94, "");
        physical_fs_id = io.add_filesystem(physical_fs);

        file_system_inst rom_fs = create_rom_filesystem(nullptr, mem.get(), epocver::epoc94, "");
        rom_fs_id = io.add_filesystem(rom_fs);

        cpu = arm::create_core(cpu_type);
        kern = std::make_unique<kernel_system>(parent, timing.get(), &io, conf, &romf, cpu.get(),
            &asmdis);

        epoc::init_panic_descriptions();
        
#if ENABLE_SCRIPTING == 1
        load_scripts();
#endif
    }

    system_impl::system_impl(system *parent, drivers::graphics_driver *graphics_driver, drivers::audio_driver *audio_driver, config::state *conf)
        : parent(parent)
        , conf(conf)
        , debugger(nullptr)
        , gdriver(graphics_driver)
        , adriver(audio_driver)
        , exit(false) {
        cpu_type = arm::string_to_arm_emulator_type(conf->cpu_backend);
        
        // Initialize manager. It doesn't depend much on other
        mngr.init(parent, conf);
        io.init();
    }

    void system_impl::set_graphics_driver(drivers::graphics_driver *graphics_driver) {
        gdriver = graphics_driver;
    }

    void system_impl::set_audio_driver(drivers::audio_driver *aud_driver) {
        adriver = aud_driver;
    }

    bool system_impl::load(const std::u16string &path, const std::u16string &cmd_arg) {
        process_ptr pr = kern->spawn_new_process(path, cmd_arg);

        if (!pr) {
            return false;
        }

        pr->run();
        return true;
    }

    bool system_impl::pause() {
        paused = true;

        if (timing)
            timing->set_paused(true);

        return true;
    }

    bool system_impl::unpause() {
        paused = false;

        if (timing)
            timing->set_paused(false);

        return true;
    }

    int system_impl::loop() {
        if (paused) {
            return 1;
        }

        bool should_step = false;
        bool script_hits_the_feels = false;

#ifdef ENABLE_SCRIPTING
        manager::script_manager *scripter = get_manager_system()->get_script_manager();
#endif

        if (gdb_stub.is_server_enabled()) {
            gdb_stub.handle_packet();

            if (gdb_stub.get_cpu_halt_flag()) {
                if (gdb_stub.get_cpu_step_flag()) {
                    should_step = true;
                } else {
                    return 1;
                }
            }
        } else {
#ifdef ENABLE_SCRIPTING
            if (scripter->last_breakpoint_hit(kern->crr_thread())) {
                should_step = true;
                script_hits_the_feels = true;
            }
#endif
        }

        if (kern->crr_thread() == nullptr) {
            prepare_reschedule();
        } else {
            kernel::thread *thr = kern->crr_thread();

            if (!should_step) {
                cpu->run(thr->get_remaining_screenticks());
                thr->add_ticks(cpu->get_num_instruction_executed());
            } else {
                cpu->step();

#ifdef ENABLE_SCRIPTING
                if (script_hits_the_feels) {
                    should_step = true;
                    script_hits_the_feels = true;
                    scripter->reset_breakpoint_hit(cpu.get(), kern->crr_thread());
                }
#endif

                thr->add_ticks(1);
            }
        }

        if (!kern->should_terminate()) {
#ifdef ENABLE_SCRIPTING
            scripter->call_reschedules();
#endif

            kern->reschedule();

            reschedule_pending = false;
        } else {
            exit = true;
            return 0;
        }

        return 1;
    }

    bool system_impl::install_package(std::u16string path, drive_number drv) {
        std::atomic<int> h;
        return mngr.get_package_manager()->install_package(path, drv, h);
    }

    bool system_impl::load_rom(const std::string &path) {
        symfile f = eka2l1::physical_file_proxy(path, READ_MODE | BIN_MODE);

        if (!f || !f->valid()) {
            LOG_ERROR("ROM file not present: {}", path);
            return false;
        }

        eka2l1::ro_file_stream rom_fstream(f.get());
        std::optional<loader::rom> romf_res = loader::load_rom(reinterpret_cast<common::ro_stream *>(
            &rom_fstream));

        if (!romf_res) {
            return false;
        }

        romf = std::move(*romf_res);

        if (rom_fs_id) {
            io.remove_filesystem(*rom_fs_id);
        }

        auto current_device = mngr.get_device_manager()->get_current();

        if (!current_device) {
            return false;
        }

        file_system_inst rom_fs = create_rom_filesystem(&romf, mem.get(),
            get_symbian_version_use(), current_device->firmware_code);

        rom_fs_id = io.add_filesystem(rom_fs);
        bool res1 = kern->map_rom(romf.header.rom_base, path);

        if (!res1) {
            return false;
        }

        return true;
    }

    void system_impl::shutdown() {
        kern.reset();
        mem.reset();
        asmdis.shutdown();
        timing.reset();

        exit = false;
    }

    void system_impl::mount(drive_number drv, const drive_media media, std::string path,
        const std::uint32_t attrib) {
        io.mount_physical_path(drv, media, attrib, common::utf8_to_ucs2(path));
    }

    void system_impl::request_exit() {
        cpu->stop();
        exit = true;
    }

    void system_impl::reset() {
        exit = false;
    }

    bool system_impl::install_rpkg(const std::string &devices_rom_path, const std::string &path, std::string &firmware_code) {
        // TODO: Progress bar
        std::atomic_int holder;
        bool res = eka2l1::loader::install_rpkg(mngr.get_device_manager(), path, devices_rom_path,
            firmware_code, holder);

        if (!res) {
            return false;
        }

        return true;
    }

    void system_impl::add_new_hal(uint32_t hal_category, hal_instance &hal_com) {
        hals.emplace(hal_category, std::move(hal_com));
    }

    epoc::hal *system_impl::get_hal(uint32_t category) {
        return hals[category].get();
    }

    system::system(drivers::graphics_driver *gdriver, drivers::audio_driver *adriver, config::state *conf)
        : impl(new system_impl(this, gdriver, adriver, conf)) {
    }

    system::~system() {
        delete impl;
    }

    config::state *system::get_config() {
        return impl->get_config();
    }

    void system::set_config(config::state *conf) {
        impl->set_config(conf);
    }

    void system::set_graphics_driver(drivers::graphics_driver *graphics_driver) {
        return impl->set_graphics_driver(graphics_driver);
    }

    void system::set_audio_driver(drivers::audio_driver *adriver) {
        return impl->set_audio_driver(adriver);
    }

    void system::set_debugger(debugger_base *new_debugger) {
        return impl->set_debugger(new_debugger);
    }

    bool system::set_device(const std::uint8_t idx) {
        return impl->set_device(idx);
    }

    void system::set_symbian_version_use(const epocver ever) {
        return impl->set_symbian_version_use(ever);
    }

    void system::set_cpu_executor_type(const arm_emulator_type type) {
        return impl->set_cpu_executor_type(type);
    }

    const arm_emulator_type system::get_cpu_executor_type() const {
        return impl->get_cpu_executor_type();
    }

    loader::rom *system::get_rom_info() {
        return impl->get_rom_info();
    }

    epocver system::get_symbian_version_use() const {
        return impl->get_symbian_version_use();
    }

    void system::prepare_reschedule() {
        return impl->prepare_reschedule();
    }

    void system::startup() {
        return impl->startup();
    }

    bool system::load(const std::u16string &path, const std::u16string &cmd_arg) {
        return impl->load(path, cmd_arg);
    }

    int system::loop() {
        return impl->loop();
    }

    bool system::pause() {
        return impl->pause();
    }

    bool system::unpause() {
        return impl->unpause();
    }

    void system::shutdown() {
        return impl->shutdown();
    }

    manager_system *system::get_manager_system() {
        return impl->get_manager_system();
    }
    memory_system *system::get_memory_system() {
        return impl->get_memory_system();
    }

    kernel_system *system::get_kernel_system() {
        return impl->get_kernel_system();
    }

    hle::lib_manager *system::get_lib_manager() {
        return impl->get_lib_manager();
    }

    io_system *system::get_io_system() {
        return impl->get_io_system();
    }

    ntimer *system::get_ntimer() {
        return impl->get_ntimer();
    }

    disasm *system::get_disasm() {
        return impl->get_disasm();
    }

    gdbstub *system::get_gdb_stub() {
        return impl->get_gdb_stub();
    }

    drivers::graphics_driver *system::get_graphics_driver() {
        return impl->get_graphic_driver();
    }

    drivers::audio_driver *system::get_audio_driver() {
        return impl->get_audio_driver();
    }

    arm::core *system::get_cpu() {
        return impl->get_cpu();
    }

    dispatch::dispatcher *system::get_dispatcher() {
        return impl->get_dispatcher();
    }

    void system::mount(drive_number drv, const drive_media media, std::string path,
        const std::uint32_t attrib) {
        return impl->mount(drv, media, path, attrib);
    }

    void system::reset() {
        return impl->reset();
    }

    bool system::install_rpkg(const std::string &devices_rom_path, const std::string &path, std::string &firmware_code) {
        return impl->install_rpkg(devices_rom_path, path, firmware_code);
    }

    void system::load_scripts() {
        return impl->load_scripts();
    }

    /*! \brief Install a SIS/SISX. */
    bool system::install_package(std::u16string path, drive_number drv) {
        return impl->install_package(path, drv);
    }

    bool system::load_rom(const std::string &path) {
        return impl->load_rom(path);
    }

    void system::request_exit() {
        return impl->request_exit();
    }

    bool system::should_exit() const {
        return impl->should_exit();
    }

    void system::add_new_hal(uint32_t hal_category, hal_instance &hal_com) {
        return impl->add_new_hal(hal_category, hal_com);
    }

    epoc::hal *system::get_hal(uint32_t category) {
        return impl->get_hal(category);
    }

    void system::do_state(common::chunkyseri &seri) {
        return impl->do_state(seri);
    }

    const language system::get_system_language() const {
        return impl->get_system_language();
    }

    void system::set_system_language(const language new_lang) {
        impl->set_system_language(new_lang);
    }

    void system::validate_current_device() {
        impl->validate_current_device();
    }
}
