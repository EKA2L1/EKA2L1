#pragma once

#include <cstdint>
#include <vector>
#include <optional>

namespace eka2l1 {
    class system;
}

namespace eka2l1::epoc {
    /*! \brief Query all first entries of all the DLL loaded. 
     * \returns A vector contains all 32-bit address of first entries. 
     */
    std::vector<std::uint32_t> query_entries(eka2l1::system *sys);

    /*! \brief Given the entry point of an dll, get the full path */
    std::optional<std::u16string> get_dll_full_path(eka2l1::system *sys, const std::uint32_t addr);
}