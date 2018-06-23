#pragma once

#include <ipc.h>
#include <ptr.h>

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

            bool write_arg(int idx, uint32_t data);
            bool write_arg_pkg(int idx, uint8_t *data, uint32_t len);

            // Package an argument, write it to a destination
            template <typename T>
            bool write_arg_pkg(int idx, T data) {
                return write_arg_pkg(idx, reinterpret_cast<uint8_t *>(&data), sizeof(T));
            }
        };
    }
}