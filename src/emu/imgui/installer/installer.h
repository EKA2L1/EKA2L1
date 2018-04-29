#pragma once

#include <string>
#include <optional>

namespace eka2l1 {
	namespace imgui {
        std::optional<std::string> choose_sis_dialog();
        bool pop_up_warning(const std::string msg);

        bool install_sis_dialog_op();
    }
}
