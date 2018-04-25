#include "menu.h"

#include <GLFW/glfw3.h>
#include <ImguiWindowsFileIO.hpp>
#include "installer/installer.h"

namespace eka2l1 {
    namespace imgui {
        bool show_sis_dialog = false;
        bool show_vfs_rebase = false;

        bool request_dir_1 = false;
        bool request_dir_2 = false;

        std::vector<char> path_c(400);
        std::vector<char> path_e(400);

        void menu::draw() {
             if (ImGui::BeginMainMenuBar()) {
                 if (ImGui::BeginMenu("File")) {
                      if (ImGui::MenuItem("Install SIS", "Ctrl+Shift+I"))  {
                          show_sis_dialog = true;
                      }

                      if (ImGui::MenuItem("Quit")) {
                          glfwSetWindowShouldClose(window, true);
                      }

                      ImGui::EndMenu();
                 }

                 if (ImGui::BeginMenu("Edit")) {
                     if (ImGui::MenuItem("VFS Rebase")) {
                        show_vfs_rebase = true;
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

             if (show_vfs_rebase) {
                 ImVec2 new_win_size(300, 150);
                 ImGui::SetNextWindowSize(new_win_size);

                 if (ImGui::Begin("VFS Rebase", nullptr, ImGuiWindowFlags_ResizeFromAnySide)) {
                     ImGui::InputText("C:", &path_c[0], 256); ImGui::SameLine(0, 25);

                     if (ImGui::Button("...##1")) {
                         request_dir_1 = true;
                     }

                     ImGui::InputText("E:", &path_e[0], 256); ImGui::SameLine(0, 25);

                     if (ImGui::Button("...##2")) {
                         if (!request_dir_1) {
                             request_dir_2 = true;
                         }
                     }

                     ImGui::Button("OK");
                     ImGui::SameLine();
                     ImGui::Button("Cancel");

                     ImGui::End();
                 }
             }

             if (request_dir_1 || request_dir_2) {
                 std::string temp_path = "";
                 ImVec2 size = ImVec2(400 ,250);

                 bool res = dirIOWindow(temp_path, "OK", false, size);

                 if (res) {
                     if (request_dir_1) {
                         memcpy(path_c.data(), temp_path.data(), temp_path.length());
                         request_dir_1 = false;
                     } else {
                         memcpy(path_e.data(), temp_path.data(), temp_path.length());
                         request_dir_2 = false;
                     }
                 }
             }
        }
    }
}
