#pragma once

#include <common/types.h>

#include <cstdint>
#include <optional>
#include <vector>

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

    /* \brief Given a path in a code segment, find that code segment and get the exception descriptor of it. 
       \returns 0 if the given segment has no exception descriptor or the section not found.
    */
    address get_exception_descriptor_addr(eka2l1::system *sys, address runtime_addr);
}