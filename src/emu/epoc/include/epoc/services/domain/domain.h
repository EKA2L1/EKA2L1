#pragma once

#include <epoc/services/context.h>
#include <epoc/services/server.h>

#include <epoc/kernel.h>
#include <epoc/services/domain/database.h>

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace eka2l1 {
    struct domain;
    struct hierarchy;

    using domain_ptr = std::shared_ptr<domain>;
    using hierarchy_ptr = std::shared_ptr<eka2l1::hierarchy>;

    struct domain : public service::database::domain {
        friend struct hierarchy;

        domain_ptr peer;
        domain_ptr parent;
        domain_ptr child;

        int trans_timeout_event;

        int transition_count;
        int child_count;
        int state;

        bool observed;

        hierarchy_ptr hierarchy;
        property_ptr state_prop;

        std::vector<service::session *> attached_sessions;

    private:
        void do_members_transition();
        void do_children_transition();
        void do_domain_transition();

        void complete_members_transition();
        void complete_children_transition();
        void complete_domain_transition();

        bool is_notification_enabled(service::session *ss);
        void set_notification_option(service::session *ss, const bool val);

        void cancel_transition();

    public:
        void complete_acknowledge_with_err(const int err);
        void transition_timeout(uint64_t data, const int cycles_late);

        explicit domain()
            : observed(false) {}

        ~domain();

        /* Functions */
        void attach_session(service::session *ss);
        domain_ptr lookup_child(const std::uint16_t id);

        int get_previous_state() {
            return state_prop->get_int() & 0xffffff;
        }

        void set_observe(const bool observe_op);
    };

    struct hierarchy : public service::database::hierarchy {
        friend struct domain;

        eka2l1::ntimer *timing;
        eka2l1::memory_system *mem;

        domain_ptr root_domain;
        service::session *control_session;
        service::session *observe_session;

        TDmTraverseDirection traverse_dir;
        domain_ptr trans_domain;
        domain_ptr observed_domain;

        std::vector<TTransInfo> transitions;
        std::vector<TTransFailInfo> transitions_fail;

        int trans_state;
        int transition_prop_value;

        int observe_type;
        int transition_id;
        int observed_children;

        bool observer_started;

        eka2l1::ptr<epoc::request_status> trans_status;
        eka2l1::ptr<epoc::request_status> observe_status;

        thread_ptr trans_status_thr;
        thread_ptr obs_status_thr;

        /* Functions */
        domain_ptr lookup(const std::uint16_t id);

        using deferral_status = std::pair<eka2l1::ptr<epoc::request_status>, thread_ptr>;

        std::unordered_map<std::uint64_t, bool> acknowledge_pending;
        std::unordered_map<std::uint64_t, deferral_status> deferral_statuses;

    private:
        void set_state(const std::int32_t next_state, const TDmTraverseDirection new_traverse_dir);
        void add_transition(const std::uint16_t id, const int state, const int err);
        void add_transition_failure(const std::uint16_t id, const int err);

    public:
        hierarchy(memory_system *mem, ntimer *tsys)
            : timing(tsys)
            , mem(mem) {}

        bool is_observe_nof_outstanding() {
            return (bool)observe_status;
        }

        void finish_observe_request(const int err_code) {
            *(observe_status.get(mem)) = err_code;
            observe_status = 0;
        }

        void finish_trans_request(const int err_code) {
            *(trans_status.get(mem)) = err_code;
            trans_status = 0;
        }

        bool transition(eka2l1::ptr<epoc::request_status> trans_nof_sts, const std::uint32_t domain_id, const std::int32_t target_state,
            const TDmTraverseDirection dir);
    };

    struct domain_manager {
        kernel_system *kern;
        ntimer *timing;

        std::unordered_map<uint32_t, hierarchy_ptr> hierarchies;

        hierarchy_ptr lookup_hierarchy(const std::uint8_t id);
        domain_ptr lookup_domain(const std::uint8_t hierarchy_id, const std::uint16_t domain_id);

        bool add_hierarchy_from_database(const std::uint32_t id);
    };

    class domain_server : public service::server {
        friend struct domain;

        std::unordered_map<std::uint64_t, bool> nof_enable;
        std::unordered_map<std::uint64_t, domain_ptr> control_domains;

        std::shared_ptr<domain_manager> mngr;

        void join_domain(service::ipc_context &context);
        void request_transition_nof(service::ipc_context &context);
        void cancel_transition_nof(service::ipc_context &context);
        void acknowledge_last_state(service::ipc_context &context);
        void defer_acknowledge(service::ipc_context &context);
        void cancel_defer_acknowledge(service::ipc_context &context);

    public:
        explicit domain_server(eka2l1::system *sys, std::shared_ptr<domain_manager> &mngr);

        std::shared_ptr<domain_manager> &get_domain_manager() {
            return mngr;
        }
    };

    class domainmngr_server : public service::server {
        std::unordered_map<std::uint64_t, hierarchy_ptr> control_hierarchies;

        std::shared_ptr<domain_manager> mngr;

        // These hierarchies already exist in EKA2L1 database and will be taken into
        // use when this is called
        void add_new_hierarchy(service::ipc_context &context);
        void join_hierarchy(service::ipc_context &context);

        void request_domain_transition(service::ipc_context &context);
        void request_system_transition(service::ipc_context &context);
        void cancel_transition(service::ipc_context &context);
        void get_transition_fail_count(service::ipc_context &context);

        void observer_join(service::ipc_context &context);
        void observer_start(service::ipc_context &context);
        void observer_cancel(service::ipc_context &context);
        void observer_notify(service::ipc_context &context);

        void observed_count(service::ipc_context &context);

    public:
        explicit domainmngr_server(eka2l1::system *sys);

        std::shared_ptr<domain_manager> &get_domain_manager() {
            return mngr;
        }
    };
}
