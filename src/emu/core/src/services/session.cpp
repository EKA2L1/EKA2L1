#include <services/session.h>
#include <services/server.h>

namespace eka2l1 {
    namespace service {
        int session::send_receive_sync(ipc_msg_ptr &msg) {
            int local_response;

            server_msg smsg;
            smsg.msg_status = ipc_message_status::delivered;
            smsg.real_msg = msg;
            smsg.request_status = &local_response

            int deliver_success
                = svr->deliver(smsg);

            // ..................... Execute this imm

            return local_response;
        }

        int session::send_receive(ipc_msg_ptr &msg, int &status) {
            server_msg smsg;
            smsg.msg_status = ipc_message_status::delivered;
            smsg.real_msg = msg;
            smsg.request_status = &status; // No response

            return svr->deliver(msg);
        }

        // Send blind message
        int session::send(ipc_msg_ptr &msg) {
            server_msg smsg;
            smsg.msg_status = ipc_message_status::delivered;
            smsg.real_msg = msg;
            smsg.request_status = nullptr; // No response

            return svr->deliver(smsg);
        }
    }
}