#include <services/session.h>
#include <services/server.h>

#include <core_kernel.h>

namespace eka2l1 {
    namespace service {
        session::session(kernel_system *kern, server_ptr svr, int async_slot_count)
            : svr(svr), kernel_obj(kern, "", kernel::owner_type::process, 0) {
            svr->attach(this);

            if (async_slot_count > 0) {
                msgs_pool.resize(async_slot_count);

                for (auto &msg : msgs_pool) {
                    msg = kern->create_msg(kernel::owner_type::process);
                }
            }
        }

        ipc_msg_ptr &session::get_free_msg() {
            if (msgs_pool.empty()) {
                return kern->create_msg(kernel::owner_type::process);
            }

            auto &free_msg_in_pool = std::find_if(msgs_pool.begin(), msgs_pool.end(),
                [](const auto &msg) { return msg->free; });

            if (free_msg_in_pool != msgs_pool.end()) {
                return *free_msg_in_pool;
            }

            return ipc_msg_ptr(nullptr);
        }

        // This behaves a little different then other
        int session::send_receive_sync(int function, ipc_arg args, int *request_sts) {
            ipc_msg_ptr &msg = kern->crr_thread()->get_sync_msg();

            if (!msg) {
                return -1;
            }

            msg->function = function;
            msg->args = args;
            msg->own_thr = kern->crr_thread();
            msg->request_sts = request_sts;

            send_receive(msg);

            return 0;
        }

        int session::send_receive_sync(int function, ipc_arg args) {
            int local_response;
            ipc_msg_ptr &msg = kern->crr_thread()->get_sync_msg();

            if (!msg) {
                return -1;
            }

            msg->function = function;
            msg->args = args;
            msg->own_thr = kern->crr_thread();
            msg->request_sts = &local_response;

            return send_receive_sync(msg);
        }

        int session::send_receive_sync(int function) {
            int local_response;

            ipc_msg_ptr &msg = kern->crr_thread()->get_sync_msg();

            if (!msg) {
                return -1;
            }

            msg->function = function;
            msg->args = ipc_arg(0, 0, 0, 0, 0);
            msg->own_thr = kern->crr_thread();
            msg->request_sts = &local_response;

            return send_receive_sync(msg);
        }

        int session::send_receive(int function, ipc_arg args, int *request_sts) {
            ipc_msg_ptr msg = get_free_msg();

            if (!msg) {
                return -1;
            }

            msg->function = function;
            msg->args = args;
            msg->request_sts = request_sts;

            send_receive(msg);

            return 0;
        }

        int session::send_receive(int function, int *request_sts) {
            ipc_msg_ptr &msg = get_free_msg();

            if (!msg) {
                return -1;
            }

            msg->function = function;
            msg->args = ipc_arg(0, 0, 0, 0, 0);
            msg->request_sts = request_sts;

            return send_receive(msg);
        }

        int session::send(int function, ipc_arg args) {
            ipc_msg_ptr &msg = get_free_msg();

            if (!msg) {
                return -1;
            }

            msg->function = function;
            msg->args = args;

            msg->request_sts = nullptr;

            return send(msg);
        }

        int session::send(int function) {
            ipc_msg_ptr &msg = get_free_msg();

            if (!msg) {
                return -1;
            }

            msg->function = function;
            msg->args = ipc_arg(0, 0, 0, 0, 0);

            msg->request_sts = nullptr;

            return send(msg);
        }

        int session::send_receive_sync(ipc_msg_ptr &msg) {
            server_msg smsg;
            smsg.real_msg = msg;
            smsg.real_msg->msg_status = ipc_message_status::delivered;

            int deliver_success
                = svr->deliver(smsg);

            svr->process_accepted_msg();

            return *smsg.real_msg->request_sts;
        }

        int session::send_receive(ipc_msg_ptr &msg) {
            server_msg smsg;

            smsg.real_msg = msg;
            smsg.real_msg->msg_status = ipc_message_status::delivered;

            return svr->deliver(smsg);
        }

        // Send blind message
        int session::send(ipc_msg_ptr &msg) {
            server_msg smsg;

            smsg.real_msg = msg;
            smsg.real_msg->msg_status = ipc_message_status::delivered;
            smsg.real_msg->request_sts = nullptr; 

            return svr->deliver(smsg);
        }

        void session::prepare_close() {
            for (const auto &msg : msgs_pool) {
                kern->free_msg(msg);
            }
        }
    }
}