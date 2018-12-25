/*
* Copyright (c) 2018 EKA2L1 Team.
*
* This file is part of EKA2L1 project
* (see bentokun.github.com/EKA2L1).
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <common/e32inc.h>

#include <e32err.h>
#include <epoc/ptr.h>

#include <cassert>
#include <cstring>
#include <string>
#include <memory>

namespace eka2l1 {
    class system;

    namespace kernel {
        class process;
    }

    using process_ptr = std::shared_ptr<kernel::process>;
}

/*! \brief Epoc namespace is the namespace for all morden C++ implementation of 
 *         class/function in Symbian, for EKA1, EKA2 (both ^1, ^3) 
 * 
 * These can help services implementation or executive calls implementation.
 */
namespace eka2l1::epoc {
    enum des_type: std::uint32_t {
        buf_const,
        ptr_const,
        ptr,
        buf,
        ptr_to_buf
    };

    constexpr int des_err_not_large_enough_to_hold = -1;

    // These are things that we can't do within header
    struct desc_base {
    protected:
        std::uint32_t info;

    public:
        inline std::uint32_t  get_length() const {
            // 28 low bit stores the length
            return info & 0xFFFFFF;
        }

        inline des_type       get_descriptor_type() const {
            return static_cast<des_type>(info >> 28);
        }

        void *get_pointer_raw(eka2l1::process_ptr pr);
        
        int assign_raw(eka2l1::process_ptr pr, const std::uint8_t *data, 
            const std::uint32_t size);

        std::uint32_t get_max_length(eka2l1::process_ptr pr);

        void set_length(eka2l1::process_ptr pr, const std::uint32_t new_len);
    };

    template <typename T>
    struct desc: public desc_base {
    public:      
        T *get_pointer(eka2l1::process_ptr pr) {
            return reinterpret_cast<T*>(get_pointer_raw(pr));
        }

        std::basic_string<T> to_std_string(eka2l1::process_ptr pr) {
            const des_type dtype = get_descriptor_type();
            assert((dtype >= buf_const) && (dtype <= ptr_to_buf));

            std::basic_string<T> data;
            data.resize(get_length());

            T *data_pointer_guest = get_pointer(pr);
            std::copy(data_pointer_guest, data_pointer_guest + data.length(), &data[0]);

            return data;
        }

        int assign(eka2l1::process_ptr pr, const std::uint8_t *data, 
            const std::uint32_t size) {
            std::uint8_t *des_buf = reinterpret_cast<std::uint8_t*>(get_pointer_raw(pr));
            des_type dtype = get_descriptor_type();

            std::uint32_t real_len = size / sizeof(T);

            if ((dtype == buf) || (dtype == ptr) || (dtype == ptr_to_buf)) {
                if (real_len > get_max_length(pr)) {
                    return des_err_not_large_enough_to_hold;
                }
            }

            std::memcpy(des_buf, data, size);
            set_length(pr, real_len);

            return 0;
        }

        int assign(eka2l1::process_ptr pr, const std::basic_string<T> &buf) {
            return assign(pr, reinterpret_cast<const std::uint8_t*>(&buf[0]), 
                static_cast<std::uint32_t>(buf.size() * sizeof(T)));
        }
    };

    template <typename T>
    struct des: public desc<T> {
        std::uint32_t max_length;
    };

    template <typename T>
    struct ptr_desc: public desc<T> {
        eka2l1::ptr<T>  data;
    };

    template <typename T>
    struct ptr_des: public des<T> {
        eka2l1::ptr<T>  data;
    };

    template <typename T>
    struct buf_desc: public desc<T> {
        T   data[1];
    };

    template <typename T>
    struct buf_des: public des<T> {
        T   data[1];
    };

    template <typename T>
    struct literal {
        std::uint32_t   length;
        T               data[1];
    };

    using desc8 = desc<char>;
    using desc16 = desc<char16_t>;
    using des8 = des<char>;
    using des16 = des<char16_t>;

    using ptr_desc8 = ptr_desc<char>;
    using ptr_desc16 = ptr_desc<char16_t>;

    using buf_desc8 = buf_desc<char>;
    using buf_desc16 = buf_desc<char16_t>;

    using buf_des8 = buf_des<char>;
    using buf_des16 = buf_des<char16_t>;

    using ptr_des8 = ptr_des<char>;
    using ptr_des16 = ptr_des<char16_t>;

    static_assert(sizeof(desc8) == 4);
    static_assert(sizeof(desc16) == 4);
    static_assert(sizeof(des8) == 8);
    static_assert(sizeof(des16) == 8);
    static_assert(sizeof(ptr_desc8) == 8);
    static_assert(sizeof(ptr_desc16) == 8);
    static_assert(sizeof(ptr_des8) == 12);
    static_assert(sizeof(ptr_des16) == 12);
}