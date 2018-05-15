#include "menu.h"

#include "installer/installer.h"
#include <GLFW/glfw3.h>
#include <ImguiWindowsFileIO.hpp>
#include <SimpleIni.h>
#include <core/vfs.h>

namespace eka2l1 {
    namespace imgui {
        bool show_sis_dialog = false;
        bool show_vfs_rebase = false;

        bool request_dir_1 = false;
        bool request_dir_2 = false;

        std::vector<char> path_c(400);
        std::vector<char> path_e(400);

        CSimpleIniA ini;

        void remount() {
            vfs::mount("C:", path_c.data());
            vfs::mount("E:", path_e.data());
        }

        void save_vfs_path() {
            int rc = ini.SetValue("vfs", nullptr, nullptr);
            rc = ini.SetValue("vfs", "c", path_c.data());
            rc = ini.SetValue("vfs", "e", path_e.data());

            ini.SaveFile("config.ini");

            remount();
        }

        void menu::init() {
            ini.SetUnicode();
            ini.LoadFile("config.ini");

            auto path_c_temp = ini.GetValue("vfs", "c", "./drives/c/");
            auto path_e_temp = ini.GetValue("vfs", "e", "./drives/e/");

            path_c.resize(strlen(path_c_temp));
            path_e.resize(strlen(path_e_temp));

            memcpy(path_c.data(), path_c_temp, strlen(path_c_temp));
            memcpy(path_e.data(), path_e_temp, strlen(path_e_temp));            
        
            remount();
        }

        void menu::draw() {
            if (ImGui::BeginMainMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("Install SIS", "Ctrl+Shift+I")) {
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

                ImGui::Begin("VFS Rebase", nullptr, 0);

                ImGui::Text("C:  ");
                ImGui::SameLine();
                ImGui::InputText(" ", &path_c[0], 256);
                ImGui::SameLine(0, 15);

                if (ImGui::Button("...##1")) {
                    request_dir_1 = true;
                }

                ImGui::Text("E:  ");
                ImGui::SameLine();
                ImGui::InputText(" ", &path_e[0], 256);
                ImGui::SameLine(0, 15);

                if (ImGui::Button("...##2")) {
                    if (!request_dir_1) {
                        request_dir_2 = true;
                    }
                }

                ImGui::NewLine();
                ImGui::NewLine();

                ImGui::Indent(new_win_size.x / 2 - 40);

                if (ImGui::Button("OK")) {
                    save_vfs_path();
                    show_vfs_rebase = false;
                }

                ImGui::SameLine();

                if (ImGui::Button("Cancel")) {
                    show_vfs_rebase = false;
                }

                ImGui::End();
            }

            if (request_dir_1 || request_dir_2) {
                std::string temp_path = "";
                ImVec2 size = ImVec2(400, 250);

                bool res = dirIOWindow(temp_path, "OK", false, size);

                if (res) {
                    if (request_dir_1) {
                        path_c.resize(temp_path.length());
                        memcpy(path_c.data(), temp_path.data(), temp_path.length());
                        request_dir_1 = false;
                    } else {
                        path_e.resize(temp_path.length());
                        memcpy(path_e.data(), temp_path.data(), temp_path.length());
                        request_dir_2 = false;
                    }
                }
            }
        }
    }
}
