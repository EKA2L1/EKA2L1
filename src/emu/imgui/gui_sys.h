#pragma once

#include <imgui.h>
#include <ImguiWindowsFileIO.hpp>

#include <future>
#include <functional>
#include <map>
#include <vector>

// A Async GUI System based on ImGui
namespace eka2l1 {
    using draw_func = std::function<bool()>;
    using draw_callback = std::function<void(bool)>;
    using draw_id = int;

    struct draw_request {
        draw_id did;
        draw_func dfunc;
        draw_callback dcb;
    };

    class gui_system {
        std::map<draw_id, draw_request> dreqs;

    public:
        std::string open_dialogue(const std::vector<std::string> filters) {
            fileIOWindow()

            std::string res_path;
        }
    };
}
