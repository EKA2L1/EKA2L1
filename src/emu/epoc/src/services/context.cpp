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
        ipc_context::ipc_context(const bool auto_free)
            : auto_free(auto_free) {
        }

        ipc_context::~ipc_context() {
            if (auto_free) {
                msg->msg_session->set_slot_free(msg);
            }
        }

        template <typename T>
        std::enable_if_t<std::is_integral_v<T> || std::is_floating_point_v<T>, std::optional<T>>
        get_integral_arg_from_msg(ipc_msg_ptr &msg, const int idx) {
            if (idx >= 4) {
                return std::nullopt;
            }

            return *reinterpret_cast<T *>(&msg->args.args[idx]);
        }

        template <>
        std::optional<std::uint8_t> ipc_context::get_arg(const int idx) {
            return get_integral_arg_from_msg<std::uint8_t>(msg, idx);
        }

        template <>
        std::optional<std::uint16_t> ipc_context::get_arg(const int idx) {
            return get_integral_arg_from_msg<std::uint16_t>(msg, idx);
        }

        template <>
        std::optional<std::uint32_t> ipc_context::get_arg(const int idx) {
            return get_integral_arg_from_msg<std::uint32_t>(msg, idx);
        }

        template <>
        std::optional<std::int8_t> ipc_context::get_arg(const int idx) {
            return get_integral_arg_from_msg<std::int8_t>(msg, idx);
        }

        template <>
        std::optional<std::int16_t> ipc_context::get_arg(const int idx) {
            return get_integral_arg_from_msg<std::int16_t>(msg, idx);
        }

        template <>
        std::optional<std::int32_t> ipc_context::get_arg(const int idx) {
            return get_integral_arg_from_msg<std::int32_t>(msg, idx);
        }

        template <>
        std::optional<float> ipc_context::get_arg(const int idx) {
            return get_integral_arg_from_msg<float>(msg, idx);
        }

        template <>
        std::optional<std::u16string> ipc_context::get_arg(const int idx) {
            if (idx >= 4) {
                return std::nullopt;
            }

            const ipc_arg_type iatype = msg->args.get_arg_type(idx);
            const bool is_descriptor = (int)iatype & (int)ipc_arg_type::flag_des;
            const bool is_16_bit = (int)iatype & (int)ipc_arg_type::flag_16b;

            if (is_descriptor && is_16_bit) {
                kernel::process *own_pr = msg->own_thr->owning_process();
                eka2l1::epoc::desc16 *des = ptr<epoc::desc16>(msg->args.args[idx]).get(own_pr);

                if (!des) {
                    return std::nullopt;
                }

                return des->to_std_string(own_pr);
            }

            return std::nullopt;
        }

        template <>
        std::optional<std::string> ipc_context::get_arg(int idx) {
            if (idx >= 4) {
                return std::nullopt;
            }

            const ipc_arg_type iatype = msg->args.get_arg_type(idx);
            const bool is_descriptor = (int)iatype & (int)ipc_arg_type::flag_des;
            const bool is_16_bit = (int)iatype & (int)ipc_arg_type::flag_16b;

            // If it has descriptor flag and it doesn't have an 16-bit flag, it should be 8-bit one.
            if (is_descriptor && !is_16_bit) {
                kernel::process *own_process = msg->own_thr->owning_process();
                eka2l1::epoc::desc8 *des = ptr<epoc::desc8>(msg->args.args[idx]).get(own_process);

                if (!des) {
                    return std::nullopt;
                }

                return des->to_std_string(msg->own_thr->owning_process());
            }

            return std::nullopt;
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
            if (idx >= 0 && idx < 4 && msg->args.get_arg_type(idx) == ipc_arg_type::handle) {
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

        bool ipc_context::write_arg_pkg(int idx, const uint8_t *data, uint32_t len, int *err_code, const bool auto_shrink_to_fit) {
            if (idx >= 4 || idx < 0) {
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

                    des->assign(own_pr, data, write_size * 2);
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
            const ipc_arg_type arg_type = msg->args.get_arg_type(idx);

            if ((int)arg_type & (int)ipc_arg_type::flag_des) {
                kernel::process *own_pr = msg->own_thr->owning_process();
                eka2l1::epoc::des8 *des = ptr<epoc::des8>(msg->args.args[idx]).get(own_pr);

                return reinterpret_cast<std::uint8_t *>(des->get_pointer_raw(own_pr));
            }

            return nullptr;
        }

        std::size_t ipc_context::get_arg_max_size(int idx) {
            if (idx >= 4 || idx < 0) {
                return static_cast<std::size_t>(-1);
            }

            const ipc_arg_type arg_type = msg->args.get_arg_type(idx);

            if ((static_cast<int>(arg_type) & static_cast<int>(ipc_arg_type::unspecified))
                || (static_cast<int>(arg_type) & static_cast<int>(ipc_arg_type::handle))) {
                return 4;
            }

            // Cast it to descriptor
            kernel::process *own_pr = msg->own_thr->owning_process();
            epoc::des8 *descriptor = ptr<epoc::des8>(msg->args.args[idx]).get(own_pr);

            if (static_cast<int>(arg_type) & static_cast<int>(ipc_arg_type::flag_16b)) {
                return descriptor->get_max_length(own_pr) * 2;
            }

            return descriptor->get_max_length(own_pr);
        }

        std::size_t ipc_context::get_arg_size(int idx) {
            if (idx >= 4 || idx < 0) {
                return static_cast<std::size_t>(-1);
            }

            const ipc_arg_type arg_type = msg->args.get_arg_type(idx);

            if ((static_cast<int>(arg_type) & static_cast<int>(ipc_arg_type::unspecified))
                || (static_cast<int>(arg_type) & static_cast<int>(ipc_arg_type::handle))) {
                return 4;
            }

            // Cast it to descriptor
            kernel::process *own_pr = msg->own_thr->owning_process();
            epoc::des8 *descriptor = ptr<epoc::des8>(msg->args.args[idx]).get(own_pr);

            if (static_cast<int>(arg_type) & static_cast<int>(ipc_arg_type::flag_16b)) {
                return descriptor->get_length() * 2;
            }

            return descriptor->get_length();
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
