#include <core.h>
#include <ptr.h>

#include <hle/epoc9/des.h>

#include <services/context.h>

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
                TDesC16 *des = eka2l1::ptr<TDesC16>(msg->args.args[idx]).get(sys->get_memory_system());
                return des->StdString(sys);
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
                TDesC8 *des = eka2l1::ptr<TDesC8>(msg->args.args[idx]).get(sys->get_memory_system());
                return des->StdString(sys);
            }

            return std::optional<std::string>{};
        }

        void ipc_context::set_request_status(int res) {
            *msg->request_sts = res;
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

        bool ipc_context::write_arg_pkg(int idx, uint8_t *data, uint32_t len) {
            if (idx >= 4) {
                return false;
            }

            ipc_arg_type arg_type = msg->args.get_arg_type(idx);

            if ((int)arg_type & (int)ipc_arg_type::flag_des) {
                TDesC8 *des = eka2l1::ptr<TDesC8>(msg->args.args[idx]).get(sys->get_memory_system());
                std::string bin;

                bin.resize(len);

                memcpy(bin.data(), data, len);

                des->Assign(sys, bin);

                return true;
            } 

            return false;
        }
    }
}