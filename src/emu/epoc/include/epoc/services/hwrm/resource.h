#pragma once

namespace eka2l1::service {
    struct ipc_context;
}

namespace eka2l1::epoc {
    /**
     * \brief Base class for resource
     */
    struct resource_interface {
        virtual void execute_command(service::ipc_context &ctx) = 0;
    };
}