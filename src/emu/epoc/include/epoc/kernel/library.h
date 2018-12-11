#pragma once

#include <epoc/kernel/kernel_obj.h>

#include <memory>
#include <optional>
#include <vector>

namespace eka2l1 {
    namespace loader {
        struct romimg;
        struct e32img;

        using e32img_ptr = std::shared_ptr<e32img>;
        using romimg_ptr = std::shared_ptr<romimg>;
    }

    namespace kernel {
        class library : public kernel_obj {
            loader::romimg_ptr rom_img;
            loader::e32img_ptr e32_img;

            enum {
                rom_img_library,
                e32_img_library
            } lib_type;

            enum class library_state {
                loaded,
                attaching,
                attached
            } state;

        public:
            library(kernel_system *kern, const std::string &lib_name, loader::romimg_ptr img);
            library(kernel_system *kern, const std::string &lib_name, loader::e32img_ptr img);

            ~library() {}

            std::optional<uint32_t> get_ordinal_address(const uint8_t idx);
            std::vector<uint32_t> attach();

            bool attached();

            void write_object_to_snapshot(common::wo_buf_stream &stream) override;
            void do_state(common::ro_buf_stream &stream) override;
        };
    }
}