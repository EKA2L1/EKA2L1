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

#include <common/algorithm.h>
#include <common/chunkyseri.h>
#include <common/container.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <common/platform.h>
#include <common/random.h>

#include <disasm/disasm.h>

#include <system/consts.h>
#include <system/hal.h>
#include <system/epoc.h>

#include <utils/panic.h>

#ifdef ENABLE_SCRIPTING
#include <scripting/manager.h>
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
#include <ldd/collection.h>
#include <loader/rom.h>
#include <kernel/timing.h>
#include <services/init.h>
#include <vfs/vfs.h>

#include <cpu/arm_factory.h>
#include <cpu/arm_utils.h>

#include <config/app_settings.h>
#include <config/config.h>

#include <system/devices.h>
#include <system/software.h>
#include <package/manager.h>
#include <services/window/window.h>
#include <services/window/screen.h>

namespace eka2l1 {
    #define HAL_ENTRY(generic_name, display_name, num, num_old) hal_entry_##generic_name = num,

    enum hal_entry {
        #include <kernel/hal.def>
    };

    system_create_components::system_create_components()
        : graphics_(nullptr)
        , audio_(nullptr)
        , conf_(nullptr)
        , settings_(nullptr) {

    }
    
    class system_impl {
        std::mutex mut;

        arm::core_instance cpu;
        arm::exclusive_monitor_instance exmonitor;

        arm_emulator_type cpu_type;

        drivers::graphics_driver *gdriver;
        drivers::audio_driver *adriver;

        std::unique_ptr<memory_system> mem_;
        std::unique_ptr<kernel_system> kern_;
        std::unique_ptr<device_manager> dvcmngr_;
        std::unique_ptr<ntimer> timing_;
        std::unique_ptr<io_system> io_;
        std::unique_ptr<disasm> disassembler_;
        std::unique_ptr<gdbstub> stub_;
        std::unique_ptr<dispatch::dispatcher> dispatcher_;
        std::unique_ptr<manager::packages> packages_;

#if ENABLE_SCRIPTING
        std::unique_ptr<manager::scripts> scripting_;
#endif

        debugger_base *debugger_;
        loader::rom romf_;
        window_server *winserv_;

        config::state *conf_;
        config::app_settings *app_settings_;

        bool reschedule_pending = false;

        std::atomic<bool> exit = false;
        std::atomic<bool> paused = false;

        std::unordered_map<uint32_t, hal_instance> hals_;

        bool startup_inited = false;

        std::optional<filesystem_id> rom_fs_id_;
        std::optional<filesystem_id> physical_fs_id_;

        system *parent_;

        std::size_t gdb_stub_breakpoint_callback_handle_;
        std::size_t ldd_request_load_callback_handle_;

        common::identity_container<system_reset_callback_type> reset_callbacks_;

    public:
        explicit system_impl(system *parent, system_create_components &param);

        ~system_impl() {
            // Reset dispatchers...
            dispatcher_.reset();

            // We need to clear kernel content second, since some object do references to it,
            // and if we let it go in destructor it would be messy! :D
            if (kern_)
                kern_->wipeout();

#if ENABLE_SCRIPTING
            scripting_.reset();
#endif

            kern_.reset();
            mem_.reset();
            timing_.reset();
        };

        void set_graphics_driver(drivers::graphics_driver *graphics_driver);
        void set_audio_driver(drivers::audio_driver *audio_driver);

        void set_debugger(debugger_base *new_debugger) {
            debugger_ = new_debugger;
        }

        void setup_outsider() {
            service::init_services(parent_);

            // Try to set system language
            set_system_language(static_cast<language>(conf_->language));

            epoc::init_hal(parent_);

            // Initialize HLE finally
            dispatcher_ = std::make_unique<dispatch::dispatcher>(kern_.get(), timing_.get());

            winserv_ = reinterpret_cast<window_server *>(kern_->get_by_name<service::server>(eka2l1::get_winserv_name_by_epocver(
                kern_->get_epoc_version())));
            packages_->var_resolver = [&](const int int_val) -> int {
                switch (int_val) {
                // HAL Display X
                case hal_entry_display_screen_x_pixels:
                    return winserv_->get_screen(0)->size().x;

                case hal_entry_display_screen_y_pixels:
                    return winserv_->get_screen(0)->size().y;

                case hal_entry_machine_uid: {
                    device *dvc = get_device_manager()->get_current();
                    return dvc ? dvc->machine_uid : 0;
                }
                }

                return 0;
            };

            packages_->choose_lang = [&](const int *langs, const int count) {
                device *dvc = get_device_manager()->get_current();
                if (!dvc) {
                    LOG_WARN(SYSTEM, "No device is currently available, language choosen for package is: {}", num_to_lang(langs[0]));
                    return langs[0];
                }

                const int current_lang = get_config()->language;

                // First up, pick the device's current language
                auto lang_ite = std::find(langs, langs + count, current_lang);
                if (lang_ite != langs + count) {
                    LOG_INFO(SYSTEM, "Picked language for package: {}", num_to_lang(current_lang));
                    return current_lang;
                }

                // Pick the device's default language code
                lang_ite = std::find(langs, langs + count, dvc->default_language_code);
                if (lang_ite != langs + count) {
                    LOG_INFO(SYSTEM, "Picked language for package: {}", num_to_lang(dvc->default_language_code));
                    return dvc->default_language_code;
                }

                LOG_INFO(SYSTEM, "Picked language for package: {}", num_to_lang(langs[0]));
                return langs[0];
            };

            if (!stub_->is_server_enabled() && conf_->enable_gdbstub) {
                stub_->init(kern_.get(), io_.get());
                stub_->toggle_server(true);
            }
            
            if (stub_->is_server_enabled()) {
                gdb_stub_breakpoint_callback_handle_ = kern_->register_breakpoint_hit_callback([this](arm::core *cpu_core, kernel::thread *target,
                    const std::uint32_t addr) {
                    if (stub_->is_connected()) {
                        cpu_core->stop();
                        cpu_core->set_pc(addr);

                        cpu_core->save_context(target->get_thread_context());

                        stub_->break_exec();
                        stub_->send_trap_gdb(target, 5);
                    }
                });
            }

            ldd_request_load_callback_handle_ = kern_->register_ldd_factory_request_callback(
                &ldd::get_factory_func);
        }

        void set_symbian_version_use(const epocver ever) {
            io_->set_epoc_ver(ever);

            // Use flexible model on 9.5 and onwards.
            mem_ = std::make_unique<memory_system>(exmonitor.get(), conf_, (ever >= epocver::epoc95) ?
                mem::mem_model_type::flexible : mem::mem_model_type::multiple, is_epocver_eka1(ever) ? true : false);

            io_->install_memory(mem_.get());

            // Install memory to the kernel, then set epoc version
            kern_->install_memory(mem_.get());
            kern_->set_epoc_version(ever);
        }

        void start_access() {
            paused = true;

            if (kern_) {
                kern_->stop_cores_idling();
            }

            mut.lock();
        }

        void end_access() {
            paused = false;
            mut.unlock();
        }

        bool set_device(const std::uint8_t idx) {
            start_access();
            bool result = dvcmngr_->set_current(idx);

            if (!result) {
                end_access();
                return false;
            }

            if (!reset(false)) {
                end_access();
                return false;
            }

            end_access();
            return true;
        }
        
        bool rescan_devices(const drive_number romdrv) {
            bool actually_found = false;

            {
                start_access();

                std::optional<eka2l1::drive> drventry = io_->get_drive_entry(romdrv);
                if (!drventry.has_value()) {
                    end_access();
                    return false;
                }

                dvcmngr_->clear();

                common::dir_iterator ite(drventry->real_path);
                ite.detail = true;

                common::dir_entry firm_entry;

                while (ite.next_entry(firm_entry) == 0) {
                    if ((firm_entry.type == common::file_type::FILE_DIRECTORY) && (firm_entry.name != ".")
                        && (firm_entry.name != "..")) {
                        const std::string full_entry_path = eka2l1::add_path(drventry->real_path,
                            firm_entry.name);

                        const epocver ver = loader::determine_rpkg_symbian_version(full_entry_path);

                        std::string manu, firm_name, model;
                        loader::determine_rpkg_product_info(full_entry_path, manu, firm_name, model);

                        LOG_INFO(SYSTEM, "Found a device: {} ({})", model, firm_name);

                        if (!dvcmngr_->add_new_device(firm_name, model, manu, ver, 0)) {
                            LOG_ERROR(SYSTEM, "Unable to add this device, silently ignore!");
                        } else {
                            actually_found = true;
                        }
                    }
                }

                dvcmngr_->save_devices();
                end_access();
            }

            if (actually_found) {
                conf_->device = 0;
                conf_->serialize();

                set_device(0);
            }

            return true;
        }

        void validate_current_device() {
            io_->validate_for_host();
        }
        
        std::size_t add_system_reset_callback(system_reset_callback_type type) {
            return reset_callbacks_.add(type);
        }

        bool remove_system_reset_callback(const std::size_t h) {
            return reset_callbacks_.remove(h);
        }

        void invoke_system_reset_callbacks() {
            for (auto cb: reset_callbacks_) {
                cb(parent_);
            }
        }

        void set_cpu_executor_type(const arm_emulator_type type) {
            cpu_type = type;
        }

        const arm_emulator_type get_cpu_executor_type() const {
            return cpu_type;
        }

        config::state *get_config() {
            return conf_;
        }

        device_manager *get_device_manager() {
            return dvcmngr_.get();
        }

        manager::packages *get_packages() {
            return packages_.get();
        }

        manager::scripts *get_scripts() {
#if ENABLE_SCRIPTING
            return scripting_.get();
#else
            return nullptr;
#endif
        }

        void set_config(config::state *confs) {
            conf_ = confs;
        }

        loader::rom *get_rom_info() {
            return &romf_;
        }

        epocver get_symbian_version_use() const {
            return kern_->get_epoc_version();
        }

        void prepare_reschedule() {
            cpu->stop();
            reschedule_pending = true;
        }

        const language get_system_language() const {
            return kern_->get_current_language();
        }

        void set_system_language(const language new_lang) {
            kern_->set_current_language(new_lang);
        }

        void startup();
        bool load(const std::u16string &path, const std::u16string &cmd_arg);
        int loop();

        bool pause();
        bool unpause();

        memory_system *get_memory_system() {
            return mem_.get();
        }

        kernel_system *get_kernel_system() {
            return kern_.get();
        }

        hle::lib_manager *get_lib_manager() {
            return kern_->get_lib_manager();
        }

        io_system *get_io_system() {
            return io_.get();
        }

        ntimer *get_ntimer() {
            return timing_.get();
        }

        disasm *get_disasm() {
            return disassembler_.get();
        }

        gdbstub *get_gdb_stub() {
            return stub_.get();
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
            return dispatcher_.get();
        }

        void mount(drive_number drv, const drive_media media, std::string path,
            const std::uint32_t attrib = io_attrib_none);

        bool reset(const bool lock_sys);

        void load_scripts();
        void do_state(common::chunkyseri &seri);

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
            const std::string ext = path_extension(scripts_entry.name);

            if ((scripts_entry.type == common::FILE_REGULAR) &&  ((ext== ".py") || (ext == ".lua"))) {
                auto module_name = filename(scripts_entry.name);
                scripting_->import_module(".//scripts//" + module_name);
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
        timing_ = std::make_unique<ntimer>(DEFAULT_CPU_HZ);
        timing_->set_realtime_level(get_realtime_level_from_string(conf_->rtos_level.c_str()));

        file_system_inst physical_fs = create_physical_filesystem(epocver::epoc94, "");
        physical_fs_id_ = io_->add_filesystem(physical_fs);

        exmonitor = arm::create_exclusive_monitor(cpu_type, 1);
        cpu = arm::create_core(exmonitor.get(), cpu_type);

        kern_ = std::make_unique<kernel_system>(parent_, timing_.get(), io_.get(), conf_, app_settings_, &romf_, cpu.get(),
            disassembler_.get());

        epoc::init_panic_descriptions();
        
#if ENABLE_SCRIPTING == 1
        load_scripts();
#endif
    }

    system_impl::system_impl(system *parent, system_create_components &param)
        : parent_(parent)
        , debugger_(nullptr)
        , gdriver(param.graphics_)
        , adriver(param.audio_)
        , conf_(param.conf_)
        , app_settings_(param.settings_)
        , exit(false) {
#if EKA2L1_ARCH(ARM)
        cpu_type = arm_emulator_type::r12l1;
#else
        cpu_type = arm::string_to_arm_emulator_type(conf_->cpu_backend);
#endif
        dvcmngr_ = std::make_unique<device_manager>(conf_);

        disassembler_ = std::make_unique<disasm>();
        io_ = std::make_unique<io_system>();
        stub_ = std::make_unique<gdbstub>();
        packages_ = std::make_unique<manager::packages>(io_.get(), conf_);

#if ENABLE_SCRIPTING
        scripting_ = std::make_unique<manager::scripts>(parent);
#endif
    }

    void system_impl::set_graphics_driver(drivers::graphics_driver *graphics_driver) {
        gdriver = graphics_driver;
    }

    void system_impl::set_audio_driver(drivers::audio_driver *aud_driver) {
        adriver = aud_driver;
    }

    bool system_impl::load(const std::u16string &path, const std::u16string &cmd_arg) {
        process_ptr pr = kern_->spawn_new_process(path, cmd_arg);

        if (!pr) {
            return false;
        }

        pr->run();
        return true;
    }

    bool system_impl::pause() {
        paused = true;

        if (kern_)
            kern_->stop_cores_idling();

        const std::lock_guard<std::mutex> guard(mut);

        if (timing_)
            timing_->set_paused(true);

        return true;
    }

    bool system_impl::unpause() {
        paused = false;

        const std::lock_guard<std::mutex> guard(mut);

        if (timing_)
            timing_->set_paused(false);

        return true;
    }

    int system_impl::loop() {
        const std::lock_guard<std::mutex> guard(mut);

        if (paused) {
            return 1;
        }

        bool should_step = false;
        bool script_hits_the_feels = false;

        kernel::thread *to_run = kern_->crr_thread();

#ifdef ENABLE_SCRIPTING
        manager::scripts *scripter = get_scripts();
#endif

        if (stub_->is_server_enabled()) {
            stub_->handle_packet();

            if (stub_->get_cpu_halt_flag()) {
                if (stub_->get_cpu_step_flag()) {
                    should_step = true;
                } else {
                    return 1;
                }
            }
        } else {
#ifdef ENABLE_SCRIPTING
            if (scripter->last_breakpoint_hit(to_run)) {
                // About to run this thread, so reset the hit
                script_hits_the_feels = true;
                should_step = true;
            }
#endif

            if (conf_->stepping && !should_step) {
                should_step = true;
            }
        }

        if (to_run == nullptr) {
            prepare_reschedule();
        } else {
            if (!should_step) {
                cpu->run(to_run->get_remaining_screenticks());
            } else {
                cpu->step();

#ifdef ENABLE_SCRIPTING
                if (script_hits_the_feels)
                    scripter->reset_breakpoint_hit(cpu.get(), to_run);
#endif
            }

            to_run->add_ticks(cpu->get_num_instruction_executed());
        }

        if (!kern_->should_terminate()) {
            kern_->reschedule();
            reschedule_pending = false;
        } else {
            exit = true;
            return 0;
        }

        return 1;
    }

    bool system_impl::install_package(std::u16string path, drive_number drv) {
        return packages_->install_package(path, drv);
    }

    bool system_impl::load_rom(const std::string &path) {
        symfile f = eka2l1::physical_file_proxy(path, READ_MODE | BIN_MODE);

        if (!f || !f->valid()) {
            LOG_ERROR(SYSTEM, "ROM file not present: {}", path);
            return false;
        }

        eka2l1::ro_file_stream rom_fstream(f.get());
        std::optional<loader::rom> romf_res = loader::load_rom(reinterpret_cast<common::ro_stream *>(
            &rom_fstream));

        if (!romf_res) {
            return false;
        }

        rom_fstream.seek(0, common::seek_where::beg);

        romf_ = std::move(*romf_res);

        if (!rom_fs_id_.has_value()) {
            auto current_device = dvcmngr_->get_current();

            if (!current_device) {
                return false;
            }

            file_system_inst rom_fs = create_rom_filesystem(&romf_, mem_.get(),
                get_symbian_version_use(), current_device->firmware_code);

            rom_fs_id_ = io_->add_filesystem(rom_fs);
        }

        bool res1 = kern_->map_rom(romf_.header.rom_base, path);

        if (!res1) {
            return false;
        }

        return true;
    }

    void system_impl::mount(drive_number drv, const drive_media media, std::string path,
        const std::uint32_t attrib) {
        io_->mount_physical_path(drv, media, attrib, common::utf8_to_ucs2(path));
    }

    void system_impl::request_exit() {
        cpu->stop();
        exit = true;
    }

    bool system_impl::reset(const bool lock_sys) {
        if (lock_sys) {
            start_access();
        }

        exit = false;

        // Unregister HLE stuffs
        kern_->unregister_ldd_factory_request_callback(ldd_request_load_callback_handle_);
        kern_->unregister_breakpoint_hit_callback(gdb_stub_breakpoint_callback_handle_);

        dispatcher_.reset();
        
        if (kern_) {
            kern_->reset();
        }

        if (timing_) {
            timing_->reset();
        }

        hals_.clear();
    
        device *dvc = dvcmngr_->get_current();

        if (conf_->language == -1) {
            conf_->language = dvc->default_language_code;
            conf_->serialize();
        }

        io_->set_product_code(dvc->firmware_code);
        set_symbian_version_use(dvc->ver);

        cpu->clear_instruction_cache();
        
        // Load ROM
        const std::string rom_path = add_path(conf_->storage, add_path(preset::ROM_FOLDER_PATH, add_path(
            common::lowercase_string(dvc->firmware_code), preset::ROM_FILENAME)));

        if (!load_rom(rom_path)) {
            if (lock_sys) {
                end_access();
            }

            return false;
        }

        // Setup outsiders
        setup_outsider();
        invoke_system_reset_callbacks();

        if (lock_sys) {
            end_access();
        }

        return true;
    }

    void system_impl::add_new_hal(uint32_t hal_category, hal_instance &hal_com) {
        hals_.emplace(hal_category, std::move(hal_com));
    }

    epoc::hal *system_impl::get_hal(uint32_t category) {
        return hals_[category].get();
    }

    system::system(system_create_components &comp)
        : impl(new system_impl(this, comp)) {
    }

    system::~system() {
        delete impl;
    }

    config::state *system::get_config() {
        return impl->get_config();
    }

    device_manager *system::get_device_manager() {
        return impl->get_device_manager();
    }

    manager::packages *system::get_packages() {
        return impl->get_packages();
    }

    manager::scripts *system::get_scripts() {
        return impl->get_scripts();
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

    bool system::reset() {
        return impl->reset(true);
    }

    void system::load_scripts() {
        return impl->load_scripts();
    }

    bool system::install_package(std::u16string path, drive_number drv) {
        return impl->install_package(path, drv);
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
    
    std::size_t system::add_system_reset_callback(system_reset_callback_type cb) {
        return impl->add_system_reset_callback(cb);
    }

    bool system::remove_system_reset_callback(const std::size_t h) {
        return impl->remove_system_reset_callback(h);
    }

    bool system::rescan_devices(const drive_number romdrv) {
        return impl->rescan_devices(romdrv);
    }
}
