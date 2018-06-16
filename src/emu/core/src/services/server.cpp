#include <common/log.h>
#include <services/server.h>

namespace eka2l1 {
    namespace service {
        /* 
            Send and receive sync.
        */
        int server::send_receive(ipc_msg &msg) {
            auto &res = ipc_funcs.find(msg.function);

            if (res == ipc_funcs.end()) {
                LOG_WARN("IPC call not found: {}", msg.function);
                return -2; // KErrNotFound
            }

            std::string &name = res->second.name;
            ipc_func_wrapper &wrapper = res->second.wrapper;

            uint32_t id = res->first;

            LOG_INFO("Calling IPC: {}, id: {}", name, id);

            wrapper(msg);

            return 0;
        }

        /* 
               Send the blind message only. It doesnt expect to receive anything
        */
        int server::send(ipc_msg &msg) {
            return send_receive(msg);
        }
    }
}