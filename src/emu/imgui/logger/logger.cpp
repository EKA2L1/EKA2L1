#include "logger.h"

namespace eka2l1 {
    namespace imgui {
        void logger::clear() {
            line_offsets.clear();
            text_buffer.clear();
        }

        void logger::log(const char *fmt, ...) {
            int old_size = text_buffer.size();
            va_list args;
            va_start(args, fmt);
            text_buffer.appendfv(fmt, args);
            va_end(args);

            for (int new_size = text_buffer.size(); old_size < new_size; old_size++)
                if (text_buffer[old_size] == '\n')
                    line_offsets.push_back(old_size);

            scroll_to_bottom = true;
        }

        void logger::draw(const char *title, bool *p_opened) {
            ImGui::SetNextWindowSize(ImVec2(1500, 900), ImGuiSetCond_FirstUseEver);
            ImGui::Begin(title, p_opened, ImGuiWindowFlags_ResizeFromAnySide);
            if (ImGui::Button("Clear"))
                clear();
            ImGui::SameLine();
            bool copy = ImGui::Button("Copy");
            ImGui::SameLine();
            text_filter.Draw("Filter", -100.0f);
            ImGui::Separator();
            ImGui::BeginChild("scrolling");
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
            if (copy)
                ImGui::LogToClipboard();

            if (text_filter.IsActive()) {
                const char *buf_begin = text_buffer.begin();
                const char *line = buf_begin;
                for (int line_no = 0; line != NULL; line_no++) {
                    const char *line_end = (line_no < line_offsets.size()) ? buf_begin + line_offsets[line_no] : NULL;
                    if (text_filter.PassFilter(line, line_end))
                        ImGui::TextUnformatted(line, line_end);
                    line = line_end && line_end[1] ? line_end + 1 : NULL;
                }
            } else {
                ImGui::TextUnformatted(text_buffer.begin());
            }

            if (scroll_to_bottom)
                ImGui::SetScrollHere(1.0f);
            scroll_to_bottom = false;
            ImGui::PopStyleVar();
            ImGui::EndChild();
            ImGui::End();
        }
    }
}
