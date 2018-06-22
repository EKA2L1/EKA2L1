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

        /*! \brief Property is a kind of environment data. 
		 *
		 * Property is defined by cagetory and key. Each property contains either
		 * integer or binary data. Properties are stored in the kernel until shutdown,
		 * and they are the way of ITC (Inter-Thread communication).
 		 *
		*/
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

			/*! \brief Set the property value (integer).
             *
             * If the property type is not integer, this return false, else
             * it will set the value and notify the request.		
             *
             * \param val The value to set.		 
			*/
            bool set(int val);
			
			
			/*! \brief Set the property value (bin).
             *
             * If the property type is not binary, this return false, else
             * it will set the value and notify the request.	
             * 
             * \param data The pointer to binary data.	
             * \param arr_length The binary data length.
             *
             * \returns false if arr_length exceeds allocated data length or property is not binary.			 
			*/
            bool set(uint8_t *data, uint32_t arr_length);

            int get_int();
            std::vector<uint8_t> get_bin();

			/*! \brief Notify the request that there is data change */
            void notify_request();
        };
    }
}