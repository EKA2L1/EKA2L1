#include <common/log.h>
#include <core.h>
#include <services/server.h>

namespace eka2l1 {
    namespace service {
        bool server::is_msg_delivered(ipc_msg_ptr &msg) {
            auto &res = std::find_if(delivered_msgs.begin(), delivered_msgs.end(),
                [&](const auto &svr_msg) { return svr_msg.real_msg->id == msg->id; });

            if (res != delivered_msgs.end()) {
                return true;
            }

            return false;
        }

        // Create a server with name
        server::server(system *sys, const std::string name)
            : sys(sys)
            , kernel_obj(sys->get_kernel_system(), name, kernel::owner_type::process, 0) {
            kernel_system *kern = sys->get_kernel_system();
            process_msg = kern->create_msg(kernel::owner_type::process);
        }

        int server::receive(ipc_msg_ptr &msg) {
            /* If there is pending message, pop the last one and accept it */
            if (!delivered_msgs.empty()) {
                server_msg yet_pending = std::move(delivered_msgs.front());

                yet_pending.dest_msg = msg;

                accept(yet_pending);
                delivered_msgs.pop_back();
            }

            return 0;
        }

        int server::accept(server_msg msg) {
            // Server is dead, nope, i wont accept you, never, or maybe ...
            if (owning_thread->current_state() == kernel::thread_state::stop) {
                return -1;
            }

            msg.dest_msg->msg_status = ipc_message_status::accepted;
            msg.real_msg->msg_status = ipc_message_status::accepted;

            msg.dest_msg->args = msg.real_msg->args;
            msg.dest_msg->function = msg.real_msg->function;
            msg.dest_msg->request_sts = msg.real_msg->request_sts;

            // Mark the client sending message as free
            kern->free_msg(msg.dest_msg);

            return 0;
        }

        int server::deliver(server_msg msg) {
            // Is ready
            if (msg.is_ready()) {
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
            receive(process_msg);

            int func = process_msg->function;

            auto func_ite = ipc_funcs.find(func);

            if (func_ite == ipc_funcs.end()) {
                return;
            }

            ipc_func ipf = func_ite->second;
            ipc_context context{ sys, process_msg };

            LOG_INFO("Calling IPC: {}, id: {}", ipf.name, func);

            ipf.wrapper(context);

            // Not sure if this is finished
            *process_msg->request_sts = 0;

            // Signal request semaphore, to tell everyone that it has finished random request
            process_msg->own_thr->signal_request();
        }

        void server::destroy() {
            sys->get_kernel_system()->destroy_msg(process_msg);
        }
    }
}