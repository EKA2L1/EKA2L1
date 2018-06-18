#include <ipc.h>

namespace eka2l1 {
    namespace service {
        ipc_arg::ipc_arg(int arg0, const int aflag) {
            arg[0] = arg0;
            flag = aflag;
        }

        ipc_arg::ipc_arg(int arg0, int arg1, const int aflag) {
            arg[0] = arg0;
            arg[1] = arg1;
            flag = aflag;
        }

        ipc_arg::ipc_arg(int arg0, int arg1, int arg2, const int aflag) {
            arg[0] = arg0;
            arg[1] = arg1;
            arg[2] = arg2;

            flag = aflag;
        }

        ipc_arg::ipc_arg(int arg0, int arg1, int arg2, int arg3, const int aflag) {
            arg[0] = arg0;
            arg[1] = arg1;
            arg[2] = arg2;
            arg[3] = arg3;

            flag = aflag;
        }

        ipc_arg_type ipc_arg::get_arg_type(int slot) {
            return static_cast<ipc_arg_type>((flag >> (slot * 3)) & 7);
        }
    }
}