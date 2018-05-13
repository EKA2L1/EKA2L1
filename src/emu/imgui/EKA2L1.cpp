#include "EKA2L1.h"

#include "loader/eka2img.h"
#include "loader/sis.h"

#include "imguirdr.h"
#include "internal.h"
#include "io/imguiio.h"
#include "logger/logger.h"
#include "window.h"

#include <common/data_displayer.h>
#include <core/core.h>
#include <core/vfs.h>

#include "installer/installer.h"

static void glfw_err_callback(int err, const char *des) {
    std::cout << err << ": " << des << std::endl;
}

namespace eka2l1 {
    namespace imgui {
        eka2l1_inst::eka2l1_inst(int width, int height) {
            glfwSetErrorCallback(glfw_err_callback);

            bool res = eka2l1::imgui::init();

            if (!res) {
                throw std::runtime_error("Can not intialize the emulator's GLFW!");
            }

            emu_win = eka2l1::imgui::open_window("EKA2L1", width, height);
            emu_menu = eka2l1::imgui::menu(emu_win);

            eka2l1::imgui::setup_io(emu_win);

            ImGui::StyleColorsDark();

            mem_dumper = std::make_shared<imgui_mem_dumper>();
            eka2l1::data_dump_setup(mem_dumper);

            debug_logger = std::make_shared<eka2l1::imgui::logger>();
            eka2l1::log::setup_log(debug_logger);

            vfs::init();

            // Intialize core
            eka2l1::core::init();
            LOG_INFO("EKA2L1: Experimental Symbian SIS Emulator");
        }

        eka2l1_inst::~eka2l1_inst() {
            imgui::destroy_window(emu_win);
            imgui::free_gl();

            core::shutdown();
            vfs::shutdown();

            glfwTerminate();
        }

        void eka2l1_inst::run() {
            //core::load("color", 0xDDDDDDDD, "/home/dtt2502/Miscs/EKA2L1HW.exe");

            while (!glfwWindowShouldClose(emu_win)) {
                glfwPollEvents();

                eka2l1::core::loop();

                eka2l1::imgui::update_io(emu_win);
                eka2l1::imgui::newframe_gl(emu_win);

                emu_menu.draw();
                mem_dumper->draw();
                debug_logger->draw("EKA2L1 Logger");

                eka2l1::dump_data_map("Memory", eka2l1::core_mem::get_addr<uint8_t>(0x70000000), 0x50000, 0x70000000);
                eka2l1::imgui::clear_gl(clear_color);

                ImGui::Render();
                eka2l1::imgui::draw_gl(ImGui::GetDrawData());
                glfwSwapBuffers(emu_win);
            }
        }
    }
}
