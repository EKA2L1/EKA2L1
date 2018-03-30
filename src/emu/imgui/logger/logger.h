#pragma once

#include <imgui.h>
#include <common/log.h>
#include <vector>

#include <cstdint>

namespace eka2l1 {
    namespace imgui {
        class logger: public eka2l1::base_logger {
             std::vector<uint32_t> line_offsets;

             ImGuiTextBuffer text_buffer;
             ImGuiTextFilter text_filter;

             bool scroll_to_bottom;

        public:
             void clear() override;
             void log(const char* fmt, ...) override;

             void draw(const char* title, bool* p_opened = NULL);
        };
    }
}
