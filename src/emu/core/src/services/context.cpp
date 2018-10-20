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

#include <core/core.h>
#include <core/ptr.h>

#include <core/epoc/des.h>

#include <core/services/context.h>

namespace eka2l1 {
    namespace service {
        template <>
        std::optional<int> ipc_context::get_arg(int idx) {
            if (idx >= 4) {
                return std::optional<int>{};
            }

            return msg->args.args[idx];
        }

        template <>
        std::optional<std::u16string> ipc_context::get_arg(int idx) {
            if (idx >= 4) {
                return std::optional<std::u16string>{};
            }

            ipc_arg_type iatype = msg->args.get_arg_type(idx);

            if ((int)iatype & ((int)ipc_arg_type::flag_des | (int)ipc_arg_type::flag_16b)) {
                eka2l1::epoc::TDesC16 *des = static_cast<eka2l1::epoc::TDesC16 *>(msg->own_thr->owning_process()->get_ptr_on_addr_space(msg->args.args[idx]));

                return des->StdString(msg->own_thr->owning_process());
            }

            return std::optional<std::u16string>{};
        }

        template <>
        std::optional<std::string> ipc_context::get_arg(int idx) {
            if (idx >= 4) {
                return std::optional<std::string>{};
            }

            ipc_arg_type iatype = msg->args.get_arg_type(idx);

            if ((int)iatype & (int)ipc_arg_type::flag_des) {
                eka2l1::epoc::TDesC8 *des = static_cast<eka2l1::epoc::TDesC8 *>(
                    msg->own_thr->owning_process()->get_ptr_on_addr_space(msg->args.args[idx]));

                return des->StdString(msg->own_thr->owning_process());
            }

            return std::optional<std::string>{};
        }

        void ipc_context::set_request_status(int res) {
            if (msg->request_sts) {
                *msg->request_sts = res;
            }

            // Avoid signal twice to cause undefined behavior
            if (!signaled) {
                signaled = true;
                msg->own_thr->signal_request();
            }
        }

        int ipc_context::flag() const {
            return msg->args.flag;
        }

        bool ipc_context::write_arg(int idx, uint32_t data) {
            if (idx < 4 && msg->args.get_arg_type(idx) == ipc_arg_type::handle) {
                msg->args.args[idx] = data;
                return true;
            }

            return false;
        }

        bool ipc_context::write_arg(int idx, const std::u16string &data) {
            if (idx >= 4) {
                return false;
            }

            ipc_arg_type arg_type = msg->args.get_arg_type(idx);

            if ((int)arg_type & ((int)ipc_arg_type::flag_des | (int)ipc_arg_type::flag_16b)) {
                eka2l1::epoc::TDesC16 *des = static_cast<eka2l1::epoc::TDesC16 *>(
                    msg->own_thr->owning_process()->get_ptr_on_addr_space(msg->args.args[idx]));
                
                des->Assign(msg->own_thr->owning_process(), data);

                return true;
            }

            return false;
        }

        bool ipc_context::write_arg_pkg(int idx, uint8_t *data, uint32_t len) {
            if (idx >= 4) {
                return false;
            }

            ipc_arg_type arg_type = msg->args.get_arg_type(idx);

            if ((int)arg_type & (int)ipc_arg_type::flag_des) {
                eka2l1::epoc::TDesC8 *des = static_cast<eka2l1::epoc::TDesC8 *>(
                    msg->own_thr->owning_process()->get_ptr_on_addr_space(msg->args.args[idx]));
                
                std::string bin;
                bin.resize(len);

                memcpy(bin.data(), data, len);
                des->Assign(msg->own_thr->owning_process(), bin);

                return true;
            }

            return false;
        }
    }
}