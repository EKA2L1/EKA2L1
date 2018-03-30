#pragma once

#include "logger/logger.h"
#include "internal.h"
#include "imguirdr.h"
#include "menu.h"

#include <iostream>

namespace eka2l1 {
    namespace imgui {
        class eka2l1_inst {
            std::shared_ptr<logger> debug_logger;
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
