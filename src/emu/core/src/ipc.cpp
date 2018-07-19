#include <core/ipc.h>

namespace eka2l1 {
    ipc_arg::ipc_arg(int arg0, const int aflag) {
        args[0] = arg0;
        flag = aflag;
    }

    ipc_arg::ipc_arg(int arg0, int arg1, const int aflag) {
        args[0] = arg0;
        args[1] = arg1;
        flag = aflag;
    }

    ipc_arg::ipc_arg(int arg0, int arg1, int arg2, const int aflag) {
        args[0] = arg0;
        args[1] = arg1;
        args[2] = arg2;

        flag = aflag;
    }

    ipc_arg::ipc_arg(int arg0, int arg1, int arg2, int arg3, const int aflag) {
        args[0] = arg0;
        args[1] = arg1;
        args[2] = arg2;
        args[3] = arg3;

        flag = aflag;
    }

    ipc_arg_type ipc_arg::get_arg_type(int slot) {
        return static_cast<ipc_arg_type>((flag >> (slot * 3)) & 7);
    }
}