#pragma once

#include <kernel/kernel_obj.h>
#include <ptr.h>

#include <array>
#include <vector>

namespace eka2l1 {
    namespace service {
        enum class property_type {
            int_data,
            bin_data
        };

        /*! \brief Property is a kind of environment data. */
        class property: public kernel::kernel_obj, public std::pair<int, int> {
        protected:
            union {
                int ndata;
               std::array<uint8_t, 512> bindata;
            } data;

            uint32_t data_len;
            uint32_t bin_data_len;

            service::property_type data_type;

        public:
            property(kernel_system *kern, service::property_type pt, uint32_t pre_allocated);

            bool set(int val);
            bool set(uint8_t *data, uint32_t arr_length);

            int get_int();
            std::vector<uint8_t> get_bin();

            void notify_request();
        };
    }
}