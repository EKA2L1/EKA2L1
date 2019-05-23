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

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include <common/queue.h>
#include <debugger/debugger.h>

struct MemoryEditor;

namespace eka2l1 {
    class system;
    struct imgui_logger;

    class imgui_debugger : public debugger_base {
        system *sys;
        manager::config_state *conf;

        bool should_show_threads;
        bool should_show_mutexs;
        bool should_show_chunks;

        bool should_pause;
        bool should_stop;
        bool should_load_state;
        bool should_save_state;
        bool should_package_manager;
        bool should_show_disassembler;
        bool should_show_logger;
        bool should_show_breakpoint_list;
        bool should_show_preferences;

        bool should_package_manager_display_file_list;
        bool should_package_manager_remove;

        const char *installer_text;
        bool installer_text_result { false };

        std::condition_variable installer_cond;
        std::mutex installer_mut;

        const int *installer_langs;
        int installer_lang_size;
        int installer_lang_choose_result { -1 };
        int installer_current_lang_idx { -1 };

        bool should_package_manager_display_installer_text;
        bool should_package_manager_display_language_choose;

        void show_threads();
        void show_mutexs();
        void show_chunks();
        void show_timers();
        void show_disassembler();
        void show_menu();
        void show_breakpoint_list();
        void show_preferences();
        void show_package_manager();

        void show_pref_personalisation();
        void show_pref_general();
        void show_pref_mounting();
        void show_pref_hal();

        void show_installer_text_popup();
        void show_installer_choose_lang_popup();

        std::unique_ptr<std::thread> install_thread;
        threadsafe_cn_queue<std::string> install_list;
        std::mutex install_thread_mut;
        std::condition_variable install_thread_cond;

        bool install_thread_should_stop = false;

        std::size_t cur_pref_tab{ 0 };
        std::atomic<std::uint32_t> debug_thread_id;

        std::shared_ptr<MemoryEditor> mem_editor;
        std::shared_ptr<imgui_logger> logger;

        std::uint32_t addr = 0;
        std::uint32_t selected_package_index = 0;
        std::int32_t modify_element = -1;

        std::mutex debug_lock;
        std::condition_variable debug_cv;

    public:
        explicit imgui_debugger(eka2l1::system *sys, std::shared_ptr<imgui_logger> logger);
        ~imgui_debugger();

        bool should_emulate_stop() override {
            return should_stop;
        }

        system *get_sys() const {
            return sys;
        }

        void show_debugger(std::uint32_t width, std::uint32_t height, std::uint32_t fb_width, std::uint32_t fb_height) override;
        void handle_shortcuts();

        void wait_for_debugger() override;
        void notify_clients() override;
        
        manager::config_state *get_config() override;
    };
}
