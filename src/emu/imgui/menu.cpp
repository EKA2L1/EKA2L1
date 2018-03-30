#include "menu.h"

#include <GLFW/glfw3.h>

namespace eka2l1 {
    namespace imgui {
        void menu::draw() {
             if (ImGui::BeginMainMenuBar()) {
                 if (ImGui::BeginMenu("File")) {
                      if (ImGui::MenuItem("Install SIS"))  {
                          ImGui::Text("Symbian Package Manager Installer");
                      }

                      if (ImGui::MenuItem("Quit")) {
                          glfwSetWindowShouldClose(window, true);
                      }

                      ImGui::EndMenu();
                 }

                 ImGui::EndMainMenuBar();
             }
        }
    }
}
