/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
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

#include <debugger/imgui_debugger.h>
#include <debugger/renderer/common.h>

#include <common/language.h>
#include <system/epoc.h>

#include <imgui.h>

namespace eka2l1 {
    void imgui_debugger::show_about() {
        if (!phony_icon) {
            static constexpr const char *PHONY_PATH = "resources\\phony.png";
            phony_icon = renderer::load_texture_from_file_standalone(sys->get_graphics_driver(),
                PHONY_PATH, false, &phony_size.x, &phony_size.y);
        }

        if (ImGui::Begin("About!", &should_show_about)) {
            ImGui::Columns(2);

            if (phony_icon) {
                ImVec2 the_pos = ImGui::GetWindowSize();
                the_pos.x = ImGui::GetColumnWidth();

                the_pos.x = (the_pos.x - phony_size.x) / 2.0f;
                the_pos.y = (the_pos.y - phony_size.y) / 2.0f;

                ImGui::SetCursorPos(the_pos);
                ImGui::Image(reinterpret_cast<ImTextureID>(phony_icon), ImVec2(static_cast<float>(phony_size.x), static_cast<float>(phony_size.y)));
            }

            ImGui::NextColumn();

            if (ImGui::BeginChild("##EKA2L1CreditsText")) {
                const std::string title = common::get_localised_string(localised_strings, "credits_title");
                const std::string copyright = common::get_localised_string(localised_strings, "credits_copyright_text");
                const std::string thanks = common::get_localised_string(localised_strings, "credits_thanks_text");

                ImGui::Text("%s", title.c_str());
                ImGui::Separator();

                ImGui::Text("%s", copyright.c_str());
                ImGui::Text("%s", thanks.c_str());
                ImGui::Separator();

                const std::string main_dev_str = common::get_localised_string(localised_strings, "credits_main_developer_text");
                ImGui::Text("%s", main_dev_str.c_str());

                for (auto &dev: main_dev_strings) {
                    ImGui::Text("- %s", dev.c_str());
                }

                ImGui::Separator();

                const std::string contrib_str = common::get_localised_string(localised_strings, "credits_contributors_text");
                ImGui::Text("%s", contrib_str.c_str());

                for (auto &contriber: contributors_strings) {
                    ImGui::Text("- %s", contriber.c_str());
                }
                
                ImGui::Separator();

                const std::string honor_str = common::get_localised_string(localised_strings, "credits_honors_text");
                ImGui::Text("%s", honor_str.c_str());

                for (auto &honor: honors_strings) {
                    ImGui::Text("- %s", honor.c_str());
                }

                ImGui::Separator();

                const std::string translator_str = common::get_localised_string(localised_strings, "credits_translators_text");
                ImGui::Text("%s", translator_str.c_str());

                for (auto &trans: translators_strings) {
                    ImGui::Text("- %s", trans.c_str());
                }

                ImGui::EndChild();
            }

            ImGui::Columns(1);

            ImGui::End();
        }
    }
}