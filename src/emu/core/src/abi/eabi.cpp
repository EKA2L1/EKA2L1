#include <abi/eabi.h>
#include <common/advstream.h>
#include <common/log.h>

#include <vector>
#include <mutex>
#include <sstream>

namespace eka2l1 {
    // This is not exactly an ABI, but a ABI-like 
    // implementation of some Symbian specific stuffs
    namespace eabi {
        // Some functions leave.
        struct leave_handler {
            std::string leave_msg;
            uint32_t leave_id;
        };

        std::vector<leave_handler> leaves;
        std::mutex mut;

        void leave(uint32_t id, const std::string& msg)
        {
            std::lock_guard<std::mutex> guard(mut);
            leave_handler handler;

            handler.leave_id = id;
            handler.leave_msg = msg;

            leaves.push_back(handler);

            LOG_CRITICAL("Process leaved! ID: {}, Message: {}", id, msg);
        }

        void trap_leave(uint32_t id) {
            auto res = std::find_if(leaves.begin(), leaves.end(),
                [id](auto lh) { return lh.leave_id == id; });

            if (res != leaves.end()) {
                std::lock_guard<std::mutex> guard(mut);
                leaves.erase(res);

                LOG_INFO("Leave ID {} has been trapped", id);
            }
        }

        void pure_virtual_call() {
            LOG_CRITICAL("Weird behavior: A pure virtual call called.");
        };

        void deleted_virtual_call() {
            LOG_CRITICAL("Weird behavior: A deleted virtual call called!");
        }
    }
}