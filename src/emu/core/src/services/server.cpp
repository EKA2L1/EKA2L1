#include <common/log.h>
#include <services/server.h>

namespace eka2l1 {
    namespace service {
        bool server::is_msg_delivered(ipc_msg_ptr &msg) {
            auto &res = std::find_if(delivered_msgs.begin(), delivered_msgs.end(),
                [&](const auto &svr_msg) { return svr_msg.real_msg->id == msg->id; });

            if (res != delivered_msgs.end()) {
                return true;
            }

            auto &res2 = std::find_if(accepted_msgs.begin(), accepted_msgs.end(),
                [&](const auto &svr_msg) { return svr_msg.real_msg->id == msg->id; });

            if (res2 != accepted_msgs.end()) {
                return true;
            }

            return false;
        }

        int server::receive(ipc_msg_ptr &msg, int &request_sts) {
            msg.status = request_sts;

            /* Message received. Count as delivered*/
            server_msg smsg;
            smsg.request_status = &request_status;
            smsg.msg_status = ipc_message_status::delivered;
            smsg.real_msg = msg;

            /* If there is pending message, pop the last one and accept it */
            if (!delivered_msgs.empty()) {
                server_msg yet_pending = std::move(delivered_msgs.front());
                accept(yet_pending.real_msg);
                delivered_msgs.pop_back();
            }

            /* Push the received message to the delivered messages */
            delivered_msgs.push_back(smsg);

            return 0;
        }

        int server::accept(server_msg msg) {
            // Server is dead, nope, i wont accept you, never, or maybe ...
            if (owning_thread->current_state() == kernel::thread_state::stop) {
                return -1;
            }

            msg.msg_status = ipc_message_status::accepted;
            accepted_msgs.push_back(std::move(msg));

            return 0;
        }

        int server::deliver(server_msg msg) {
            // Is ready
            if (msg.status == 0) {
                accept(msg);
            } else {
                delivered_msgs.push_back(std::move(msg));
            }

            return 0;
        }

        int server::cancel() {
            delivered_msgs.pop_back();

            return 0;
        }

        void server::register_ipc_func(uint32_t ordinal, ipc_func func) {
            ipc_funcs.emplace(ordinal, func);
        }

        // Processed asynchronously, use for HLE service where accepted function
        // is fetched imm
        void server::process_accepted_msg() {
            if (accepted_msgs.size() == 0) {
                return;
            }

            auto &msg = std::move(accepted_msgs.back());
        }
    }
}