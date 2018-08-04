#pragma once

#include <vector>

namespace eka2l1 {
    class system;
}

namespace eka2l1::epoc {
    /*! \brief Query all first entries of all the DLL loaded. 
     * \returns A vector contains all 32-bit address of first entries. 
     */
    std::vector<uint32_t> query_entries(eka2l1::system *sys);
}