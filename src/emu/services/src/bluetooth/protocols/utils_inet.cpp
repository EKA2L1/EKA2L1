#include <services/bluetooth/protocols/utils_inet.h>

namespace eka2l1::epoc::bt {
    void run_task_on(std::shared_ptr<uvw::loop> loop, std::function<void()> task) {
        auto async_op = loop->resource<uvw::async_handle>();

        async_op->on<uvw::async_event>([async_op, task](const uvw::async_event &event, uvw::async_handle &handle) {
            // Force async op to be used
            auto async_op_copy = async_op;
            task();

            async_op_copy.reset();
        });

        async_op->send();
    }
}