#pragma once

#include <debugger/analysis/subroutine.h>
#include <vector>

#include <core/loader/eka2img.h>
#include <core/loader/romimage.h>

namespace eka2l1 {
    class disasm;

    class memory_system;
}

namespace eka2l1::analysis {
    std::vector<subroutine> analysis_image(const loader::eka2img &img, disasm *asmdis
        , memory_system *mem);
    std::vector<subroutine> analysis_image(const loader::romimg &img, disasm *asmdis
        , memory_system *mem);
}