#pragma once

#include <vector>

namespace eka2l1 {
    class system;
}

namespace eka2l1::epoc {
    std::vector<uint32_t> query_entries(eka2l1::system *sys);
}