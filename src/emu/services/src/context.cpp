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

#include <system/epoc.h>
#include <kernel/kernel.h>
#include <kernel/server.h>
#include <mem/ptr.h>

#include <services/context.h>
#include <utils/des.h>
#include <utils/sec.h>

#include <config/config.h>

namespace eka2l1 {
    namespace service {
        ipc_context::ipc_context() = default;

        ipc_context::~ipc_context() {
            if (auto_deref)
                msg->unref();
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
        std::optional<std::uint8_t> ipc_context::get_argument_value(const int idx) {
            return get_integral_arg_from_msg<std::uint8_t>(msg, idx);
        }

        template <>
        std::optional<std::uint16_t> ipc_context::get_argument_value(const int idx) {
            return get_integral_arg_from_msg<std::uint16_t>(msg, idx);
        }

        template <>
        std::optional<std::uint32_t> ipc_context::get_argument_value(const int idx) {
            return get_integral_arg_from_msg<std::uint32_t>(msg, idx);
        }

        template <>
        std::optional<std::int8_t> ipc_context::get_argument_value(const int idx) {
            return get_integral_arg_from_msg<std::int8_t>(msg, idx);
        }

        template <>
        std::optional<std::int16_t> ipc_context::get_argument_value(const int idx) {
            return get_integral_arg_from_msg<std::int16_t>(msg, idx);
        }

        template <>
        std::optional<std::int32_t> ipc_context::get_argument_value(const int idx) {
            return get_integral_arg_from_msg<std::int32_t>(msg, idx);
        }

        template <>
        std::optional<float> ipc_context::get_argument_value(const int idx) {
            return get_integral_arg_from_msg<float>(msg, idx);
        }

        template <>
        std::optional<double> ipc_context::get_argument_value(const int idx) {
            return get_integral_arg_from_msg<double>(msg, idx);
        }

        template <>
        std::optional<std::u16string> ipc_context::get_argument_value(const int idx) {
            if (idx >= 4) {
                return std::nullopt;
            }

            const ipc_arg_type iatype = msg->args.get_arg_type(idx);
            const bool is_descriptor = (int)iatype & (int)ipc_arg_type::flag_des;
            const bool is_16_bit = (int)iatype & (int)ipc_arg_type::flag_16b;

            if (sys->get_kernel_system()->is_eka1() || (is_descriptor && is_16_bit)) {
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
        std::optional<std::string> ipc_context::get_argument_value(const int idx) {
            if (idx >= 4) {
                return std::nullopt;
            }

            const ipc_arg_type iatype = msg->args.get_arg_type(idx);
            const bool is_descriptor = (int)iatype & (int)ipc_arg_type::flag_des;
            const bool is_16_bit = (int)iatype & (int)ipc_arg_type::flag_16b;

            // If it has descriptor flag and it doesn't have an 16-bit flag, it should be 8-bit one.
            if (sys->get_kernel_system()->is_eka1() || (is_descriptor && !is_16_bit)) {
                kernel::process *own_process = msg->own_thr->owning_process();
                eka2l1::epoc::desc8 *des = ptr<epoc::desc8>(msg->args.args[idx]).get(own_process);

                if (!des) {
                    return std::nullopt;
                }

                return des->to_std_string(msg->own_thr->owning_process());
            }

            return std::nullopt;
        }

        void ipc_context::complete(int res) {
            if (msg->request_sts) {
                kernel_system *kern = sys->get_kernel_system();
                (msg->request_sts.get(msg->own_thr->owning_process()))->set(res, kern->is_eka1());

                // Avoid signal twice to cause undefined behavior
                if (!signaled) {
                    msg->own_thr->signal_request();
                    signaled = true;
                }
            }
        }

        int ipc_context::flag() const {
            return msg->args.flag;
        }

        bool ipc_context::write_arg(int idx, uint32_t data) {
            if (idx >= 0 && idx < 4 && (sys->get_kernel_system()->is_eka1() || (msg->args.get_arg_type(idx) == ipc_arg_type::handle))) {
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

            if (sys->get_kernel_system()->is_eka1() || ((int)arg_type & ((int)ipc_arg_type::flag_des | (int)ipc_arg_type::flag_16b))) {
                eka2l1::epoc::desc16 *des = ptr<epoc::desc16>(msg->args.args[idx]).get(msg->own_thr->owning_process());

                des->assign(msg->own_thr->owning_process(), data);

                return true;
            }

            return false;
        }

        bool ipc_context::write_data_to_descriptor_argument(int idx, const uint8_t *data, uint32_t len, int *err_code, const bool auto_shrink_to_fit) {
            if (idx >= 4 || idx < 0) {
                return false;
            }

            ipc_arg_type arg_type = msg->args.get_arg_type(idx);
            const bool is_eka1 = sys->get_kernel_system()->is_eka1();

            if (is_eka1 || ((int)arg_type & (int)ipc_arg_type::flag_des)) {
                kernel::process *own_pr = msg->own_thr->owning_process();

                // Please don't change the order
                // On EKA1 force write argument package to be 8-bit descriptor only
                if (!is_eka1 && ((int)arg_type & (int)ipc_arg_type::flag_16b)) {
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

        std::uint8_t *ipc_context::get_descriptor_argument_ptr(int idx) {
            const ipc_arg_type arg_type = msg->args.get_arg_type(idx);

            if (sys->get_kernel_system()->is_eka1() || ((int)arg_type & (int)ipc_arg_type::flag_des)) {
                kernel::process *own_pr = msg->own_thr->owning_process();
                eka2l1::epoc::des8 *des = ptr<epoc::des8>(msg->args.args[idx]).get(own_pr);

                if (!des) {
                    return nullptr;
                }

                return reinterpret_cast<std::uint8_t *>(des->get_pointer_raw(own_pr));
            }

            return nullptr;
        }

        std::size_t ipc_context::get_argument_max_data_size(int idx) {
            if (idx >= 4 || idx < 0) {
                return static_cast<std::size_t>(-1);
            }

            const ipc_arg_type arg_type = msg->args.get_arg_type(idx);
            const bool is_eka1 = sys->get_kernel_system()->is_eka1();

            if (!is_eka1) {
                if ((static_cast<int>(arg_type) & static_cast<int>(ipc_arg_type::unspecified))
                    || (static_cast<int>(arg_type) & static_cast<int>(ipc_arg_type::handle))) {
                    return 4;
                }
            }

            // Cast it to descriptor
            kernel::process *own_pr = msg->own_thr->owning_process();
            epoc::des8 *descriptor = ptr<epoc::des8>(msg->args.args[idx]).get(own_pr);

            if (!is_eka1 && (static_cast<int>(arg_type) & static_cast<int>(ipc_arg_type::flag_16b))) {
                return descriptor->get_max_length(own_pr) * 2;
            }

            return descriptor->get_max_length(own_pr);
        }

        std::size_t ipc_context::get_argument_data_size(int idx) {
            if (idx >= 4 || idx < 0) {
                return static_cast<std::size_t>(-1);
            }

            const ipc_arg_type arg_type = msg->args.get_arg_type(idx);
            const bool is_eka1 = sys->get_kernel_system()->is_eka1();

            if (!is_eka1) {
                if ((static_cast<int>(arg_type) & static_cast<int>(ipc_arg_type::unspecified))
                    || (static_cast<int>(arg_type) & static_cast<int>(ipc_arg_type::handle))) {
                    return 4;
                }
            }

            // Cast it to descriptor
            kernel::process *own_pr = msg->own_thr->owning_process();
            epoc::des8 *descriptor = ptr<epoc::des8>(msg->args.args[idx]).get(own_pr);

            if (!is_eka1 && (static_cast<int>(arg_type) & static_cast<int>(ipc_arg_type::flag_16b))) {
                return descriptor->get_length() * 2;
            }

            return descriptor->get_length();
        }

        bool ipc_context::set_descriptor_argument_length(const int idx, const std::uint32_t len) {
            ipc_arg_type arg_type = msg->args.get_arg_type(idx);

            if (sys->get_kernel_system()->is_eka1() || ((int)arg_type & (int)ipc_arg_type::flag_des)) {
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
        
        void server::connect(service::ipc_context &ctx) {
            ctx.complete(0);
        }

        void server::disconnect(service::ipc_context &ctx) {
            ctx.complete(0);
        }

        // Processed asynchronously, use for HLE service where accepted function
        // is fetched imm
        void server::process_accepted_msg() {
            ipc_msg_ptr process_msg = nullptr;
            receive(process_msg);

            if (!process_msg) {
                return;
            }

            int func = process_msg->function;

            auto func_ite = ipc_funcs.find(func);
            config::state *conf = sys->get_config();

            if (func_ite == ipc_funcs.end()) {
                if (unhandle_callback_enable) {
                    ipc_context context;

                    context.sys = sys;
                    context.msg = process_msg;

                    on_unhandled_opcode(context);

                    return;
                }

                LOG_WARN(SERVICE_TRACK, "Unimplemented IPC call: 0x{:x} for server: {}", func, obj_name);
                return;
            }

            ipc_func ipf = func_ite->second;
            ipc_context context;
            context.sys = sys;
            context.msg = process_msg;

            if (conf->log_ipc) {
                LOG_INFO(SERVICE_TRACK, "Calling IPC: {}, id: {}", ipf.name, func);
            }

            ipf.wrapper(context);
        }
    }
}
