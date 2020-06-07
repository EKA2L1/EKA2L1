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

#include <utils/des.h>
#include <common/log.h>

namespace eka2l1::epoc {
    std::uint32_t desc_base::get_max_length(eka2l1::kernel::process *pr) {
        des_type dtype = get_descriptor_type();

        switch (dtype) {
        case buf_const:
        case ptr_const: {
            return get_length();
        }

        case buf:
        case ptr:
        case ptr_to_buf: {
            auto *base = reinterpret_cast<des<std::uint8_t> *>(this);
            return base->max_length;
        }

        default: {
            break;
        }
        }

        return 0;
    }

    void desc_base::set_length(eka2l1::kernel::process *pr, const std::uint32_t new_len) {
        des_type dtype = get_descriptor_type();

        if ((dtype == buf) || (dtype == ptr) || (dtype == ptr_to_buf)) {
            const std::uint32_t max_len = get_max_length(pr);

            if (new_len > max_len) {
                LOG_ERROR("Length being set greater than maximum (max {} vs set {})",
                    max_len, new_len);
            }
        }

        info = (info & ~(0xFFFFFF)) | new_len;

        if (dtype == ptr_to_buf) {
            // We need to set the length inside.
            ptr_des<std::uint8_t> *pbuf = reinterpret_cast<decltype(pbuf)>(this);
            buf_desc<std::uint8_t> *hbufc = pbuf->data.cast<buf_desc<std::uint8_t>>().get(pr);

            hbufc->set_length(pr, new_len);
        }
    }

    void *desc_base::get_pointer_raw(eka2l1::kernel::process *pr) {
        des_type dtype = get_descriptor_type();

        switch (dtype) {
        case ptr_const: {
            ptr_desc<std::uint8_t> *des = reinterpret_cast<decltype(des)>(this);
            return des->data.cast<void>().get(pr);
        }

        case ptr: {
            ptr_des<std::uint8_t> *des = reinterpret_cast<decltype(des)>(this);
            return des->data.cast<void>().get(pr);
        }

        case buf_const: {
            buf_desc<std::uint8_t> *des = reinterpret_cast<decltype(des)>(this);
            return &(des->data[0]);
        }

        case buf: {
            buf_des<std::uint8_t> *des = reinterpret_cast<decltype(des)>(this);
            return &(des->data[0]);
        }

        case ptr_to_buf: {
            ptr_des<std::uint8_t> *pbuf = reinterpret_cast<decltype(pbuf)>(this);
            buf_desc<std::uint8_t> *hbufc = pbuf->data.cast<buf_desc<std::uint8_t>>().get(pr);

            return &(hbufc->data[0]);
        }

        default:
            break;
        }

        return nullptr;
    }
}
