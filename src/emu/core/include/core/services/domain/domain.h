#pragma once

#include <core/services/server.h>
#include <core/services/context.h>

#include <memory>
#include <unordered_map>

namespace eka2l1 {
    struct domain;
    using domain_ptr = std::shared_ptr<domain>;

    struct hierarchy {
        domain_ptr root_domain;
        domain_ptr lookup(const uint32_t id);
    };

    using hierarchy_ptr = std::shared_ptr<hierarchy>;

    struct domainmngr: public service::server {        
        std::unordered_map<uint32_t, hierarchy_ptr> hierarchies;

        hierarchy_ptr lookup(const uint32_t id);
        domain_ptr lookup_domain(const uint32_t hierarchy_id, const uint32_t domain_id);
    };

    class domain_server: public service::server {
        
    };

    class domainmngr_server: public service::server {
        void add_hierarchy(service::ipc_context context);
        void join_hierarchy(service::ipc_context context);

        void request_transtition(service::ipc_context context);

    public:
        bool add_new_hierarchy(const uint32_t id);
    };
}