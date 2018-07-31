#include <core/kernel/library.h>
#include <core/loader/eka2img.h>
#include <core/loader/romimage.h>

namespace eka2l1 {
    namespace kernel {
        library::library(kernel_system *kern, const std::string &lib_name, loader::romimg_ptr img)
            : kernel_obj(kern, lib_name, access_type::global_access) {
            obj_type = object_type::library;
            lib_type = rom_img_library;

            rom_img = img;
        }

        library::library(kernel_system *kern, const std::string &lib_name, loader::e32img_ptr img)
            : kernel_obj(kern, lib_name, access_type::global_access) {
            obj_type = object_type::library;
            lib_type = e32_img_library;

            e32_img = img;
        }

        std::optional<uint32_t> library::get_ordinal_address(const uint8_t idx) {
            if (lib_type == e32_img_library) {
                if (e32_img->ed.syms.size() <= idx) {
                    return std::optional<uint32_t>{};
                }

                return e32_img->rt_code_addr + e32_img->ed.syms[idx] - e32_img->header.code_base;
            }

            if (rom_img->exports.size() <= idx) {
                return std::optional<uint32_t>{};
            }

            return rom_img->exports[idx];
        }
    }
}