#pragma once

#include <optional>
#include <string>

namespace eka2l1 {
    class system;

    namespace imgui {
        std::optional<std::string> choose_sis_dialog();
        bool pop_up_warning(const std::string msg);

        bool install_sis_dialog_op(system* sys);
    }
}
