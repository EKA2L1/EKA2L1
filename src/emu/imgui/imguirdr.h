#pragma once

#include "internal.h"

struct ImVec4;

namespace eka2l1 {
    namespace imgui {
        void draw_gl(ImDrawData *win);
        void newframe_gl(GLFWwindow *win);
        void clear_gl(ImVec4 clcl);
        void free_gl();
    }
}
