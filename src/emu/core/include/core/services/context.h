#pragma once

#include <ipc.h>
#include <optional>

namespace eka2l1 {
    class system;

    namespace service {

        /*! Context used to pass to IPC function */
        struct ipc_context {
            eka2l1::system *sys;
            ipc_msg_ptr msg;

            template <typename T>
            std::optional<T> get_arg(int idx);

            void set_request_status(int res);

            int flag() const;
        };
    }
}