#include <epoc/services/framework.h>

namespace eka2l1::service {
    bool normal_object_container::remove(epoc::ref_count_object *obj) {
        auto res = std::lower_bound(objs.begin(), objs.end(), obj,
            [](const ref_count_object_heap_ptr &lhs, const epoc::ref_count_object *rhs) {
                return lhs->id < rhs->id;
            });

        if (res == objs.end()) {
            return false;
        }

        objs.erase(res);
        return true;
    }

    typical_server::typical_server(system *sys, const std::string name) 
        : server(sys, name, true, false) {
    }

    void typical_server::process_accepted_msg() {
        int res = receive(process_msg);

        if (res == -1) {
            return;
        }

        ipc_context context{ sys, process_msg };
        auto ss_ite = sessions.find(process_msg->msg_session->unique_id());

        if (ss_ite == sessions.end()) {
            return;
        }

        ss_ite->second->fetch(context);
    }
}
