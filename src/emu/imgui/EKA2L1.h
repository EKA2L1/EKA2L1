#pragma once

#include "logger/logger.h"
#include "memory_editor.h"
#include "internal.h"
#include "imguirdr.h"
#include "menu.h"

#include <common/data_displayer.h>

#include <iostream>

namespace eka2l1 {
    namespace imgui {
        struct imgui_mem_dumper: public eka2l1::data_dumper_interface {
            imgui_mem_dumper() = default;

            MemoryEditor editor;
            std::string name;
            std::vector<uint8_t> data;

            void set_name(const std::string& new_name) override {
                name = new_name;
            }

            void set_data(std::vector<uint8_t>& new_data) override {
                data = new_data;
            }

            void draw() override {
                editor.DrawWindow(name.c_str(), data.data(), data.size());
            }
        };

        class eka2l1_inst {
            std::shared_ptr<logger> debug_logger;
            std::shared_ptr<imgui_mem_dumper> mem_dumper;
            GLFWwindow* emu_win;
            menu emu_menu;

            ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

        public:
            eka2l1_inst(int width, int height);
            ~eka2l1_inst();
            void run();
        };
    }
}
