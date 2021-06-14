#pragma once

#include <kernel/kernel_obj.h>

#include <memory>
#include <optional>
#include <vector>

namespace eka2l1::kernel {
    class codeseg;
    using codeseg_ptr = kernel::codeseg *;

    class library : public kernel_obj {
        codeseg_ptr codeseg;
        bool reffed;

    public:
        library(kernel_system *kern, codeseg_ptr codeseg);
        ~library() {}

        void destroy() override;

        std::optional<uint32_t> get_ordinal_address(kernel::process *pr, const std::uint32_t idx);
        std::vector<uint32_t> attach(kernel::process *pr);

        bool attached(kernel::process *pr);

        codeseg_ptr get_codeseg() {
            return codeseg;
        }

        void do_state(common::chunkyseri &seri) override;
    };
}
