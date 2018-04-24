#include <manager/manager.h>
#include <ImguiWindowsFileIO.hpp>
#include <imgui.h>
#include <common/log.h>
#include <common/cvt.h>

#include "installer.h"

namespace eka2l1 {
	namespace imgui {
        bool stop_file_dialogue = false;
        bool draw_warning_box = false;

        bool pop_up_warning(const std::string msg) {
            ImGui::OpenPopup("Warning!");

            {
                if (ImGui::BeginPopup("Warning!")) {
                    ImGui::Text(msg.data());
                    if (ImGui::Button("OK")) {
                        ImGui::CloseCurrentPopup();
                        ImGui::EndPopup();
                        return true;
                    }
                    ImGui::EndPopup();
                }
            }

            return false;
        }

        std::optional<std::string> choose_sis_dialog() {
            std::string val = "";
            std::vector<std::string> filters(1);
            std::vector<std::string> rc_files(0);

            auto disize = ImVec2(450, 200);

            filters[0] = "*.sis";

            if (fileIOWindow(val, rc_files, "Install", filters, true, disize)) {
                return val;
            } else {
                if (val == "invalid") {
                    return "invalid";
                }
            }

            return std::optional<std::string>{};
		}

        bool install_sis_dialog_op() {
            std::optional<std::string> res;

            if (!stop_file_dialogue) {
                res =  imgui::choose_sis_dialog();

                if (draw_warning_box) {
                    if (imgui::pop_up_warning("Invalid file!")) {
                        draw_warning_box = false;
                    }
                }

                if (res.has_value() && res.value() != "invalid") {
                    stop_file_dialogue = true;
                    draw_warning_box = false;
                } else if (res.has_value() && res.value() == "invalid") {
                    draw_warning_box = true;
                }
            }

            if (stop_file_dialogue == true && draw_warning_box == false) {
                auto path = res.value();
                manager::get_package_manager()->install_package(
                            std::u16string(path.begin(), path.end()), 0);

                stop_file_dialogue = false;
                return true;
            }

            return false;
        }
	}

}
