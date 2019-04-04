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

#include <epoc/epoc.h>
#include <epoc/kernel.h>
#include <epoc/ptr.h>

#include <epoc/services/context.h>
#include <epoc/utils/des.h>
#include <epoc/utils/sec.h>

namespace eka2l1 {
    namespace service {
        ipc_context::~ipc_context() {
            msg->free = true;
        }

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
                kernel::process *own_pr = msg->own_thr->owning_process();
                eka2l1::epoc::desc16 *des = ptr<epoc::desc16>(msg->args.args[idx]).get(own_pr);

                return des->to_std_string(own_pr);
            }

            return std::optional<std::u16string>{};
        }

        template <>
        std::optional<std::string> ipc_context::get_arg(int idx) {
            if (idx >= 4) {
                return std::optional<std::string>{};
            }

            ipc_arg_type iatype = msg->args.get_arg_type(idx);

            // If it has descriptor flag and it doesn't have an 16-bit flag, it should be 8-bit one.
            if (((int)iatype & (int)ipc_arg_type::flag_des) && !((int)iatype & (int)ipc_arg_type::flag_16b)) {
                eka2l1::epoc::desc8 *des = ptr<epoc::desc8>(msg->args.args[idx]).get(msg->own_thr->owning_process());

                return des->to_std_string(msg->own_thr->owning_process());
            }

            return std::optional<std::string>{};
        }

        void ipc_context::set_request_status(int res) {
            if (msg->request_sts) {
                *(msg->request_sts.get(msg->own_thr->owning_process())) = res;

                // Avoid signal twice to cause undefined behavior
                if (!signaled) {
                    signaled = true;
                    msg->own_thr->signal_request();
                }
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
                eka2l1::epoc::desc16 *des = ptr<epoc::desc16>(msg->args.args[idx]).get(msg->own_thr->owning_process());

                des->assign(msg->own_thr->owning_process(), data);

                return true;
            }

            return false;
        }

        bool ipc_context::write_arg_pkg(int idx, uint8_t *data, uint32_t len, int *err_code, const bool auto_shrink_to_fit) {
            if (idx >= 4) {
                return false;
            }

            ipc_arg_type arg_type = msg->args.get_arg_type(idx);

            if ((int)arg_type & (int)ipc_arg_type::flag_des) {
                kernel::process *own_pr = msg->own_thr->owning_process();

                // Please don't change the order
                if ((int)arg_type & (int)ipc_arg_type::flag_16b) {
                    eka2l1::epoc::desc16 *des = ptr<epoc::desc16>(msg->args.args[idx]).get(own_pr);

                    // We can't handle odd length
                    assert(len % 2 == 0);

                    std::uint32_t write_size = len / 2;
                    const std::uint32_t des_to_write_size = des->get_max_length(own_pr);

                    if (auto_shrink_to_fit) {
                        write_size = common::min(write_size, des_to_write_size);
                    } else if (des_to_write_size < write_size) {
                        err_code ? (*err_code = -2) : 0;
                        return false;
                    }

                    des->assign(own_pr, data, write_size);
                } else {
                    eka2l1::epoc::des8 *des = ptr<epoc::des8>(msg->args.args[idx]).get(own_pr);

                    std::uint32_t write_size = len;
                    const std::uint32_t des_to_write_size = des->get_max_length(own_pr);

                    if (auto_shrink_to_fit) {
                        write_size = common::min(write_size, des_to_write_size);
                    } else if (des_to_write_size < write_size) {
                        err_code ? (*err_code = -2) : 0;
                        return false;
                    }

                    des->assign(own_pr, data, write_size);
                }

                return true;
            }

            (err_code) ? (*err_code = -1) : 0;
            return false;
        }

        std::uint8_t *ipc_context::get_arg_ptr(int idx) {
            ipc_arg_type arg_type = msg->args.get_arg_type(idx);

            if ((int)arg_type & (int)ipc_arg_type::flag_des) {
                kernel::process *own_pr = msg->own_thr->owning_process();
                eka2l1::epoc::des8 *des = ptr<epoc::des8>(msg->args.args[idx]).get(own_pr);

                return reinterpret_cast<std::uint8_t *>(des->get_pointer_raw(own_pr));
            }

            return nullptr;
        }

        bool ipc_context::set_arg_des_len(const int idx, const std::uint32_t len) {
            ipc_arg_type arg_type = msg->args.get_arg_type(idx);

            if ((int)arg_type & (int)ipc_arg_type::flag_des) {
                kernel::process *own_pr = msg->own_thr->owning_process();
                eka2l1::epoc::des8 *des = ptr<epoc::des8>(msg->args.args[idx]).get(own_pr);

                des->set_length(own_pr, len);
                return true;
            }

            return false;
        }

        bool ipc_context::satisfy(epoc::security_policy &policy, epoc::security_info *missing) {
            return msg->own_thr->owning_process()->satisfy(policy, missing);
        }
    }
}
