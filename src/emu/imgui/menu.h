#pragma once

#include <string>
#include <vector>

#include <imgui.h>

class GLFWwindow;

namespace eka2l1 {
    class system;

    namespace imgui {
        class menu {
            GLFWwindow *window;
            eka2l1::system* sys;

            void init();

        public:
            menu()
                : window(nullptr) { init(); }
            menu(GLFWwindow *win, eka2l1::system* sys)
                : window(win), sys(sys) { init(); }

            void draw();
        };
    }
}
