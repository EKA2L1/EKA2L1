/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <ImguiWindowsFileIO.hpp>
#include <common/cvt.h>
#include <common/log.h>
#include <core.h>
#include <imgui.h>

#include "installer.h"

namespace eka2l1 {
    namespace imgui {
        bool stop_file_dialogue = false;
        bool draw_warning_box = false;
        bool draw_success_box = false;

        bool pop_up_warning(const std::string msg) {
            ImGui::OpenPopup("Warning!");

            {
                if (ImGui::BeginPopup("Warning!")) {
                    ImGui::Text("%s", msg.data());
                    ImGui::Indent(76);
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

        bool install_sis_dialog_op(system *sys) {
            std::optional<std::string> res;

            if (!stop_file_dialogue) {
                res = imgui::choose_sis_dialog();

                if (draw_warning_box) {
                    if (pop_up_warning("Invalid file!")) {
                        draw_warning_box = false;
                    }
                }

                if (draw_success_box) {
                    if (pop_up_warning("Sucessfuly installed app!")) {
                        draw_success_box = false;
                        stop_file_dialogue = true;
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
                stop_file_dialogue = false;

                if (!res.has_value()) {
                    return true;
                }

                auto path = res.value();

                if (path == "") {
                    return true;
                }

                draw_success_box = sys->install_package(
                    std::u16string(path.begin(), path.end()), 0);
            }

            return false;
        }
    }
}

