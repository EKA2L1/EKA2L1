#pragma once

#include <epoc/kernel/kernel_obj.h>

#include <memory>
#include <optional>
#include <vector>

namespace eka2l1::kernel {
    class codeseg;
    using codeseg_ptr = kernel::codeseg*;

    class library : public kernel_obj {
        codeseg_ptr codeseg;

        enum class library_state {
            loaded,
            attaching,
            attached,
            detach_pending,
            detatching
        } state;

    public:
        library(kernel_system *kern, codeseg_ptr codeseg);
        ~library() {}

        std::optional<uint32_t> get_ordinal_address(kernel::process *pr, const std::uint32_t idx);
        std::vector<uint32_t> attach(kernel::process *pr);

        void detach(kernel::process *pr);

        bool attached();

        void do_state(common::chunkyseri &seri) override;
    };
}
