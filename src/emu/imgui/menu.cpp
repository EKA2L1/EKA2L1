#include "menu.h"

#include <GLFW/glfw3.h>
#include "installer/installer.h"

namespace eka2l1 {
    namespace imgui {
        bool show_sis_dialog = false;

        void menu::draw() {
             if (ImGui::BeginMainMenuBar()) {
                 if (ImGui::BeginMenu("File")) {
                      if (ImGui::MenuItem("Install SIS"))  {
                          show_sis_dialog = true;
                      }

                      if (ImGui::MenuItem("Quit")) {
                          glfwSetWindowShouldClose(window, true);
                      }

                      ImGui::EndMenu();
                 }

                 ImGui::EndMainMenuBar();
             }

             if (show_sis_dialog) {
                 if (imgui::install_sis_dialog_op()) {
                     show_sis_dialog = false;
                 }
             }
        }
    }
}
