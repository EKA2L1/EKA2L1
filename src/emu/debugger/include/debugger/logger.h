#pragma once

#include <common/log.h>

#include <imgui.h>

namespace eka2l1 {
    struct imgui_logger: public base_logger {
        ImGuiTextBuffer     buf;
        ImGuiTextFilter     filter;
        ImVector<int>       line_offsets;        // Index to lines offset
        bool                scroll_to_bottom;

        void clear() override;
        void log(const char *fmt, ...) override;
        void draw(const char *title, bool* p_opened = nullptr);
    };
}