#pragma once

#include <ipc.h>
#include <optional>

#define IPC_FUNC(name) void #name(eka2l1::system *sys, ipc_context ctx);

namespace eka2l1 {
    class system;

    namespace service {

        /*! Context used to pass to IPC function */
        struct ipc_context {
            eka2l1::system *sys;
            ipc_msg_ptr msg;

            template <typename T>
            std::optional<T> get_arg(int idx);

            int flag() const;
        };
    }
}