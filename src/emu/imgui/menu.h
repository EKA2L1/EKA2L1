#pragma once

#include <string>
#include <vector>

#include <imgui.h>

class GLFWwindow;

namespace eka2l1 {
    namespace imgui {
        class menu {
            GLFWwindow *window;
            void init();

        public:
            menu()
                : window(nullptr) { init(); }
            menu(GLFWwindow *win)
                : window(win) { init(); }

            void draw();
        };
    }
}
