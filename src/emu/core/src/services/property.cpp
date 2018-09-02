/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <core/core_kernel.h>
#include <core/kernel/thread.h>
#include <core/services/property.h>

#include <common/log.h>

namespace eka2l1 {
    namespace service {
        property::property(kernel_system *kern)
            : kernel::kernel_obj(kern, "", kernel::access_type::global_access) {
            obj_type = kernel::object_type::prop;
        }

        void property::define(service::property_type pt, uint32_t pre_allocated) {
            data_type = pt;
            data_len = pre_allocated;

            if (pre_allocated > 512) {
                LOG_WARN("Property trying to alloc more then 512 bytes, limited to 512 bytes");
                data_len = 512;
            }
        }

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
            if (data_type != service::property_type::int_data) {
                return -1;
            }

            return data.ndata;
        }

        std::vector<uint8_t> property::get_bin() {
            std::vector<uint8_t> local;
            local.resize(bin_data_len);

            memcpy(local.data(), data.bindata.data(), bin_data_len);

            return local;
        }

        void property::subscribe(epoc::request_status *sts) {
            if (subscribe_request.request_status) {
                return;
            }

            subscribe_request.request_thr = kern->crr_thread();
            subscribe_request.request_status = sts;
        }

        void property::cancel() {
            if (subscribe_request.request_status) {
                *subscribe_request.request_status = -3;
                subscribe_request.request_thr->signal_request();

                subscribe_request.request_status = nullptr;
            }
        }

        void property::notify_request() {
            if (subscribe_request.request_status) {
                *subscribe_request.request_status = 0;
                subscribe_request.request_thr->signal_request();

                subscribe_request.request_status = nullptr;
            }
        }
    }
}