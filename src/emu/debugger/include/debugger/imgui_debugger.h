/*
 * Copyright (c) 2020 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <common/queue.h>
#include <common/types.h>
#include <common/vecx.h>
#include <debugger/debugger.h>
#include <drivers/graphics/common.h>
#include <drivers/input/common.h>
#include <services/window/common.h>

namespace eka2l1 {
    namespace service {
        class property;
    }

    class system;
    struct imgui_logger;
    class applist_server;
    class window_server;
    class oom_ui_app_server;

    using app_launch_function = std::function<void(const std::u16string &path)>;
    using selected_window_callback_function = std::function<void(void *)>;

    class imgui_debugger : public debugger_base {
        system *sys;
        config::state *conf;

        bool should_show_threads;
        bool should_show_mutexs;
        bool should_show_chunks;
        bool should_show_window_tree;
        bool should_show_rendered_bitmap;

        bool should_pause;
        bool should_stop;
        bool should_load_state;
        bool should_save_state;
        bool should_package_manager;
        bool should_show_disassembler;
        bool should_show_logger;
        bool should_show_preferences;

        bool should_package_manager_display_file_list;
        bool should_package_manager_remove;
        bool should_install_package;

        const char *installer_text;
        bool installer_text_result{ false };

        std::condition_variable installer_cond;
        std::mutex installer_mut;

        const int *installer_langs;
        int installer_lang_size;
        int installer_lang_choose_result{ -1 };
        int installer_current_lang_idx{ -1 };

        bool should_package_manager_display_installer_text;
        bool should_package_manager_display_one_button_only;
        bool should_package_manager_display_language_choose;

        bool should_show_app_launch;
        bool should_still_focus_on_keyboard;
        bool should_show_install_device_wizard;
        bool should_show_about;

        bool should_show_empty_device_warn;

        std::uint32_t active_screen;

        struct key_binder {
            std::vector<bool> need_key;
            static constexpr int BIND_NUM = 19;
            std::array<std::uint32_t, BIND_NUM> target_key;
            std::array<std::string, BIND_NUM> target_key_name;
            std::map<std::uint32_t, std::string> key_bind_name;
        } key_binder_state;

        struct device_wizard {
            enum device_wizard_stage {
                WELCOME_MESSAGE = 0,
                SPECIFY_FILES = 1,
                INSTALL = 2,
                ENDING = 3,
                FINAL_FOR_REAL = 4
            } stage;

            std::string current_rom_path;
            std::string current_rpkg_path;

            bool should_continue;
            bool should_continue_temps[2];

            std::atomic<bool> extract_rpkg_done;
            std::atomic<bool> copy_rom_done;
            std::atomic<bool> failure;

            std::unique_ptr<std::thread> install_thread;

            explicit device_wizard()
                : stage(WELCOME_MESSAGE)
                , extract_rpkg_done(false)
                , copy_rom_done(false)
                , failure(false) {
            }
        } device_wizard_state;

        applist_server *alserv;
        window_server *winserv;
        oom_ui_app_server *oom;

        app_launch_function app_launch;

        std::mutex errors_mut;
        std::queue<std::string> error_queue;

        selected_window_callback_function selected_callback;
        void *selected_callback_data;

        void show_app_launch();
        void show_empty_device_warn();

        void show_threads();
        void show_mutexs();
        void show_chunks();
        void show_timers();
        void show_disassembler();
        void show_menu();
        void show_preferences();
        void show_package_manager();
        void show_install_device();

        void show_pref_personalisation();
        void show_pref_general();
        void show_pref_system();
        void show_pref_hal();

        void show_installer_text_popup();
        void show_installer_choose_lang_popup();
        void show_errors();
        void show_screens();

        // Server debugging
        void show_windows_tree();
        void show_rendered_bitmaps();
        void show_about();

        void set_language_to_property(const ::language new_one);

        std::unique_ptr<std::thread> install_thread;
        threadsafe_cn_queue<std::string> install_list;
        std::mutex install_thread_mut;
        std::condition_variable install_thread_cond;

        bool install_thread_should_stop = false;

        std::size_t cur_pref_tab{ 0 };
        std::atomic<std::uint64_t> debug_thread_id;

        imgui_logger *logger;

        std::uint32_t addr = 0;
        std::uint32_t selected_package_index = 0;
        std::int32_t modify_element = -1;

        std::mutex debug_lock;
        std::condition_variable debug_cv;

        drivers::handle phony_icon;
        eka2l1::vec2 phony_size;

    protected:
        void do_install_package();

    public:
        explicit imgui_debugger(eka2l1::system *sys, imgui_logger *logger, app_launch_function app_launch);
        ~imgui_debugger();

        ::language get_language_from_property(service::property *prop);

        bool should_emulate_stop() override {
            return should_stop;
        }

        system *get_sys() const {
            return sys;
        }

        std::atomic<bool> request_key;
        std::atomic<bool> key_set;
        eka2l1::drivers::input_event key_evt;

        void show_debugger(std::uint32_t width, std::uint32_t height, std::uint32_t fb_width, std::uint32_t fb_height) override;
        void queue_error(const std::string &error);
        void handle_shortcuts();

        void wait_for_debugger() override;
        void notify_clients() override;

        config::state *get_config() override;
    };
}
