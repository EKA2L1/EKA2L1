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

            if (msg->args.get_arg_type(idx) != ipc_arg_type::handle) {
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

            if (iatype == ipc_arg_type::des16 || iatype == ipc_arg_type::desc16) {
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

            if (iatype == ipc_arg_type::des8 || iatype == ipc_arg_type::desc8) {
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
    }
}