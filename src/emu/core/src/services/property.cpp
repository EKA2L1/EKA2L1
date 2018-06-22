#include <core_kernel.h>
#include <services/property.h>

namespace eka2l1 {
    namespace service {
        property::property(kernel_system *kern, service::property_type pt, uint32_t pre_allocated)
            : kernel::kernel_obj(kern, "", kernel::owner_type::process,
                  kern->get_id_base_owner(kernel::owner_type::process))
              , data_type(pt), data_len(pre_allocated) {}

        bool property::set(int val) {
            if (data_type == service::property_type::int_data) {
                data.ndata = val;
                notify_request();

                return true;
            }

            return false;
        }

        bool property::set(uint8_t *bdata, uint32_t arr_length) {
            if (arr_length > data_len) {
                return false;
            }

            memcpy(data.bindata.data(), bdata, arr_length);
            bin_data_len = arr_length;

            notify_request();

            return true;
        }

        int property::get_int() {
            return data.ndata;
        }

        std::vector<uint8_t> property::get_bin() {
            std::vector<uint8_t> local;
            local.resize(bin_data_len);

            memcpy(local.data(), data.bindata.data(), bin_data_len);

            return local;
        }

        void property::delete_prop() {
            notify_request();
        }
    }
}