#include "EKA2L1.h"

#include "loader/sis.h"
#include "loader/eka2img.h"

#include "logger/logger.h"
#include "internal.h"
#include "imguirdr.h"
#include "io/imguiio.h"
#include "window.h"

static void glfw_err_callback(int err, const char* des){
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

            debug_logger = std::make_shared<eka2l1::imgui::logger>();
            eka2l1::log::setup_log(debug_logger);

            LOG_INFO("EKA2L1: Experimental Symbian SIS Emulator");
        }

        eka2l1_inst::~eka2l1_inst() {
            eka2l1::imgui::destroy_window(emu_win);
            eka2l1::imgui::free_gl();
            glfwTerminate();
        }

        void eka2l1_inst::run() {
            auto install_finished = loader::parse_sis("/home/dtt2502/Miscs/super_miners.sis");
            auto img = loader::load_eka2img("/home/dtt2502/Miscs/creebies.exe");

            while (!glfwWindowShouldClose(emu_win)) {
                glfwPollEvents();

                eka2l1::imgui::update_io(emu_win);
                eka2l1::imgui::newframe_gl(emu_win);

                emu_menu.draw();

                debug_logger->draw("EKA2L1 Logger");
                eka2l1::imgui::clear_gl(clear_color);

                ImGui::Render();
                eka2l1::imgui::draw_gl(ImGui::GetDrawData());
                glfwSwapBuffers(emu_win);
            }
        }
    }
}
