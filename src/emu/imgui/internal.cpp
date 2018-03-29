#include "internal.h"

namespace eka2l1 {
    namespace imgui {
        bool init() {
            if (!glfwInit())
                return false;

            ImGui::CreateContext();

            return true;
        }
    }
}
