#pragma once

#include <cstdint>
#include <functional>

namespace eka2l1::loader {
    enum class epoc_sis_type {
        epocu6 = 0x1000006D,
        epoc6 = 0x10003A12
    };
    
    using show_text_func = std::function<bool(const char*)>;
    using choose_lang_func = std::function<int(const int *langs, const int count)>;
}