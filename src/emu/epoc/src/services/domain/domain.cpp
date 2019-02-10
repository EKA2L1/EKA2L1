/*
 * Copyright (c) 2018 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <epoc/services/domain/defs.h>
#include <epoc/services/domain/domain.h>

#include <epoc/epoc.h>

#include <common/cvt.h>
#include <common/e32inc.h>
#include <common/log.h>

#include <e32err.h>

#include <mutex>

namespace eka2l1 {
    /*! \brief Event triggered when the transition has reached the timout of not being acknowledged. 
     *
     * If there is at least one deferral actives, all the deferral will be finished and the timeout
     * is delayed.
     *
     * Else, this means that the domain has failed to transititon. The failure is added to the hierarchy,
     *
     * If the policy specified that the hierarchy can continue to transition another domain when the current
     * domain has failed to transition, all the pending acknowledge of the current domain will be removed.
     *
     * NOTE: In EKA2L1, there is no budget for the deferrals. You can have as many deferrals as you want. 
     * Deferrals are not limited, unlike on Symbian hardware.
     */
    void domain::transition_timeout(uint64_t data, const int cycles_late) {
        if (hierarchy->deferral_statuses.size() > 0) {
            // reschedule
            hierarchy->timing->schedule_event(trans_timeout, trans_timeout_event, data);

            // complete all deferrals
            for (auto [id, sts] : hierarchy->deferral_statuses) {
                *(sts.first.get(sts.second->owning_process())) = KErrNone;
            }

            hierarchy->deferral_statuses.clear();
            return;
        }

        hierarchy->add_transition_failure(id, KErrTimedOut);

        if (hierarchy->fail_policy == ETransitionFailureStop) {
            LOG_ERROR("Transition fail for domain {} because of timeout. Stopping because of fail policy", id);

            hierarchy->finish_trans_request(KErrTimedOut);
            cancel_transition();

            return;
        }

        if (transition_count) {
            // Fail to transition, acknowledge should not be needed anymore
            for (auto &[id, pending] : hierarchy->acknowledge_pending) {
                pending = false;
            }

            transition_count = 0;
            complete_members_transition();
        }
    }

    constexpr std::uint32_t make_state_domain_key(const std::uint32_t hier_key, const std::uint32_t domain_id) {
        return (hier_key << 8) | ((domain_id << 8) & 0xff0000) | (domain_id & 0xff);
    }

    constexpr std::uint32_t make_state_domain_value(const std::uint32_t transition_id, const std::int32_t state_val) {
        return (transition_id << 24) | (state_val & 0xffffff);
    }

    void domain::set_observe(const bool observe_op) {
        observed = observe_op;

        if (observed) {
            ++hierarchy->observed_children;
        } else {
            --hierarchy->observed_children;
        }

        if (child_count) {
            domain_ptr next = child;

            while (next) {
                next->set_observe(observe_op);
                next = next->child;
            }
        }
    }

    /*!\brief Cancel the transition
    *
    * All the children's transitions will be cancel, all deferrals
    * will be finished.
    */
    void domain::cancel_transition() {
        // Cancel all child transition first
        domain_ptr next = child;

        while (next) {
            next->cancel_transition();
            next = next->peer;
        }

        // Now cancel all pending deferrals. Acknowledge should not be
        // needed anymore so why dont we lol
        for (auto &[id, deferral_sts] : hierarchy->deferral_statuses) {
            *(deferral_sts.first.get(deferral_sts.second->owning_process())) = KErrCancel;
        }

        hierarchy->deferral_statuses.clear();
        transition_count = 0;
    }

    void domain::attach_session(session_ptr ss) {
        attached_sessions.push_back(ss);
    }

    domain::~domain() {
        hierarchy->timing->unschedule_event(trans_timeout_event, reinterpret_cast<std::uint64_t>(this));
    }

    void construct_domain_from_database(timing_system *timing, kernel_system *kern, hierarchy_ptr hier, const service::database::domain &domain_db) {
        domain_ptr dm = std::make_shared<domain>();

        std::copy(reinterpret_cast<const std::uint8_t *>(&domain_db), reinterpret_cast<const std::uint8_t *>(&domain_db) + sizeof(decltype(domain_db)),
            reinterpret_cast<std::uint8_t *>(&(*dm)));

        dm->child_count = 0;
        dm->transition_count = 0;
        dm->hierarchy = hier;

        domain_ptr parent = hier->lookup(domain_db.own_id);

        if (parent) {
            ++parent->child_count;

            dm->parent = parent;
            dm->peer = parent->child;
            parent->child = std::move(dm);
        }

        property_ptr prop = kern->create<service::property>();

        prop->first = dm_category;
        prop->second = make_state_domain_key(hier->id, domain_db.id);

        prop->define(service::property_type::int_data, 0);
        prop->set_int(make_state_domain_value(0, domain_db.init_state));

        parent->child->trans_timeout_event = timing->register_event("TransTimeoutForDomain" + common::to_string(parent->child->id),
            std::bind(&domain::transition_timeout, &(*parent->child), std::placeholders::_1, std::placeholders::_2));
    }

    hierarchy_ptr construct_hier_from_database(timing_system *timing, kernel_system *kern, const service::database::hierarchy &hier_db) {
        hierarchy_ptr hier = std::make_shared<hierarchy>(kern->get_memory_system(), timing);

        std::copy(reinterpret_cast<const std::uint8_t *>(&hier_db), reinterpret_cast<const std::uint8_t *>(&hier_db) + sizeof(decltype(hier_db)),
            reinterpret_cast<std::uint8_t *>(&(*hier)));

        hier->root_domain = std::make_shared<domain>();

        hier->root_domain->id = 0;
        hier->root_domain->parent = nullptr;
        hier->root_domain->peer = nullptr;
        hier->transition_id = 0;
        hier->observed_children = 0;
        hier->observer_started = 0;
        hier->trans_status = 0;
        hier->trans_status_thr = nullptr;
        hier->observe_status = 0;
        hier->obs_status_thr = nullptr;

        for (const auto &domain_db : hier_db.domains) {
            construct_domain_from_database(timing, kern, hier, domain_db);
        }

        return hier;
    }

    bool domain_manager::add_hierarchy_from_database(const std::uint32_t id) {
        for (const auto &hierarchy : service::database::hierarchies_db) {
            if (hierarchy.id == id) {
                hierarchies.emplace(id, construct_hier_from_database(timing, kern, hierarchy));
                return true;
            }
        }

        return false;
    }

    hierarchy_ptr domain_manager::lookup_hierarchy(const std::uint8_t id) {
        const auto &hier_ite = hierarchies.find(id);

        if (hier_ite == hierarchies.end()) {
            return nullptr;
        }

        return hier_ite->second;
    }

    domain_ptr domain_manager::lookup_domain(const std::uint8_t hierarchy_id, const std::uint16_t domain_id) {
        hierarchy_ptr hier = lookup_hierarchy(hierarchy_id);

        if (!hier) {
            return nullptr;
        }

        return hier->lookup(domain_id);
    }

    domain_ptr hierarchy::lookup(const std::uint16_t domain_id) {
        if (domain_id == 0) {
            return root_domain;
        }

        return root_domain ? root_domain->lookup_child(domain_id) : nullptr;
    }

    domain_ptr domain::lookup_child(const std::uint16_t domain_id) {
        domain_ptr next = child;

        while (next && next->id != domain_id) {
            domain_ptr dm = next->lookup_child(domain_id);

            if (dm) {
                return dm;
            }

            next = next->peer;
        }

        return next;
    }

    void domainmngr_server::add_new_hierarchy(service::ipc_context ctx) {
        const std::uint8_t hierarchy_id = static_cast<std::uint8_t>(*ctx.get_arg<int>(0));

        if (mngr->lookup_hierarchy(hierarchy_id)) {
            // Return immdiately if there is already a hierarchy.
            // Symbian doesn't set the request status to KErrAlreadyExists
            ctx.set_request_status(KErrNone);
            return;
        }

        bool res = mngr->add_hierarchy_from_database(hierarchy_id);

        if (!res) {
            ctx.set_request_status(KErrBadHierarchyId);
            return;
        }

        ctx.set_request_status(KErrNone);
    }

    void domainmngr_server::join_hierarchy(service::ipc_context ctx) {
        const std::uint8_t hierarchy_id = static_cast<std::uint8_t>(*ctx.get_arg<int>(0));
        hierarchy_ptr hier = mngr->lookup_hierarchy(hierarchy_id);

        if (!hier) {
            ctx.set_request_status(KErrBadHierarchyId);
            return;
        }

        if (hier->control_session) {
            ctx.set_request_status(KErrInUse);
            return;
        }

        hier->control_session = ctx.msg->msg_session;
        control_hierarchies[ctx.msg->msg_session->unique_id()] = hier;

        ctx.set_request_status(KErrNone);
    }

    void domain_server::join_domain(service::ipc_context ctx) {
        const std::uint32_t hierarchy_id = *ctx.get_arg<int>(0);
        const std::uint32_t domain_id = *ctx.get_arg<int>(1);

        domain_ptr domain = mngr->lookup_domain(hierarchy_id, domain_id);

        if (!domain) {
            ctx.set_request_status(KDmErrBadDomainId);
            return;
        }

        control_domains[ctx.msg->msg_session->unique_id()] = domain;
        domain->attach_session(ctx.msg->msg_session);

        ctx.set_request_status(KErrNone);
    }

    void domain_server::request_transition_nof(service::ipc_context ctx) {
        const std::uint32_t sid = ctx.msg->msg_session->unique_id();

        nof_enable[sid] = true;
        ctx.set_request_status(KErrNone);
    }

    void domain_server::cancel_transition_nof(service::ipc_context ctx) {
        const std::uint32_t sid = ctx.msg->msg_session->unique_id();

        nof_enable[sid] = false;
        ctx.set_request_status(KErrNone);
    }

    void domain_server::acknowledge_last_state(service::ipc_context ctx) {
        const int prop_val = *ctx.get_arg<int>(0);
        const int err_set = *ctx.get_arg<int>(1);

        const auto ssid = ctx.msg->msg_session->unique_id();
        domain_ptr dm = control_domains[ssid];

        if (!dm) {
            ctx.set_request_status(KDmErrNotJoin);
            return;
        }

        if (dm->hierarchy->acknowledge_pending[ssid] && dm->state_prop->get_int() == prop_val) {
            if (dm->hierarchy->deferral_statuses[ssid].first) {
                *(dm->hierarchy->deferral_statuses[ssid].first.get(
                    dm->hierarchy->deferral_statuses[ssid].second->owning_process()))
                    = KErrNone;
                dm->hierarchy->deferral_statuses[ssid].first = 0;
                dm->hierarchy->deferral_statuses[ssid].second = nullptr;
            }

            dm->complete_acknowledge_with_err(err_set);
            dm->hierarchy->acknowledge_pending[ssid] = false;

            ctx.set_request_status(KErrNone);
            return;
        }

        ctx.set_request_status(KErrNotFound);
    }

    void domain_server::defer_acknowledge(service::ipc_context ctx) {
        const std::uint32_t ssid = ctx.msg->msg_session->unique_id();
        domain_ptr dm = control_domains[ssid];

        if (!dm) {
            ctx.set_request_status(KErrNotFound);
            return;
        }

        if (dm->hierarchy->deferral_statuses[ssid].second) {
            ctx.set_request_status(KErrInUse);
            return;
        }

        if (dm->hierarchy->acknowledge_pending[ssid]) {
            dm->hierarchy->deferral_statuses.emplace(ssid,
                std::make_pair(ctx.msg->request_sts, ctx.msg->own_thr));

            return;
        }

        ctx.set_request_status(KErrNotReady);
    }

    void domain_server::cancel_defer_acknowledge(service::ipc_context ctx) {
        const std::uint32_t ssid = ctx.msg->msg_session->unique_id();
        domain_ptr dm = control_domains[ssid];

        if (!dm) {
            ctx.set_request_status(KErrNotFound);
            return;
        }

        if (dm->hierarchy->deferral_statuses[ssid].second) {
            *(dm->hierarchy->deferral_statuses[ssid].first.get(
                dm->hierarchy->deferral_statuses[ssid].second->owning_process()))
                = KErrInUse;
            dm->hierarchy->deferral_statuses[ssid].first = 0;
            dm->hierarchy->deferral_statuses[ssid].second = nullptr;
        }

        ctx.set_request_status(KErrNone);
    }

    /*! \brief Set the state for the current transition domain
     *
     * The state will be applied and changed later in the current transition
     * domain.
     * 
     * If the traverse direction is default, based on the policy, if
     * the new state is bigger than the current state in the transition domain,
     * the positive direction specified in the policy will be choosed, else
     * if will be the negative one.
    */
    void hierarchy::set_state(const std::int32_t next_state, const TDmTraverseDirection new_traverse_dir) {
        if (new_traverse_dir == ETraverseParentFirst) {
            if (next_state >= trans_domain->state) {
                traverse_dir = positive_dir;
            }

            traverse_dir = neg_dir;
        } else {
            traverse_dir = new_traverse_dir;
        }

        trans_state = next_state;
    }

    /*! \brief Do the transition, starting from a root domain defined by the given id
     * 
     * The domain will be traverse based on the traverse direction. When the transition finished,
     * the notfication will be notified, and the target state will be set in the defined property.
     *
     * Note that if a session connected to the domain enable notification, it must acknowledge the state
     * in time, or else the transition for the domain will be marked as fail.
     *
     * \returns False, most likely of bad hierarchy ID or domain ID
    */
    bool hierarchy::transition(eka2l1::ptr<epoc::request_status> trans_nof_sts, const std::uint32_t domain_id, const std::int32_t target_state,
        const TDmTraverseDirection dir) {
        domain_ptr target_domain = lookup(domain_id);

        if (!target_domain) {
            return false;
        }

        set_state(target_state, dir);

        trans_status = trans_nof_sts;
        transition_prop_value = make_state_domain_value(++transition_id, domain_id);
        target_domain->do_domain_transition();

        return true;
    }

    void hierarchy::add_transition(const std::uint16_t id, const int state, const int err) {
        transitions.push_back({ id, state, err });
    }

    void hierarchy::add_transition_failure(const std::uint16_t id, const int err) {
        transitions_fail.push_back({ id, err });
    }

    bool domain::is_notification_enabled(session_ptr ss) {
        std::shared_ptr<domain_server> dmsrv = std::reinterpret_pointer_cast<domain_server>(ss->get_server());
        return dmsrv->nof_enable[ss->unique_id()];
    }

    void domain::set_notification_option(session_ptr ss, const bool val) {
        std::shared_ptr<domain_server> dmsrv = std::reinterpret_pointer_cast<domain_server>(ss->get_server());
        dmsrv->nof_enable[ss->unique_id()] = val;
    }

    /*! \brief Do transitions for all attached sessions (member).
     *
     * If a notification is enabled in a session, that means that a transition 
     * should be proceed and need to be acknowledge in time. In that case, 
     * the notification for the session is disabled and there is an acknowledge pending
     * to be recognized by the current session before the time runs out.
     *
     * This also means that there is a transition requested. The observer will be notify
     * if there is a notification pending.
    */
    void domain::do_members_transition() {
        for (const auto &attached_session : attached_sessions) {
            if (is_notification_enabled(attached_session)) {
                ++transition_count;
                set_notification_option(attached_session, false);
                hierarchy->acknowledge_pending[attached_session->unique_id()] = true;
            }
        }

        if (observed) {
            if (hierarchy->observe_type & EDmNotifyTransRequest) {
                hierarchy->add_transition(id, get_previous_state(), KDmErrOutstanding);

                if (hierarchy->is_observe_nof_outstanding()) {
                    hierarchy->finish_observe_request(KErrNone);
                }
            }
        }

        state_prop->set(hierarchy->transition_prop_value);

        // If there is at least one client wait for transition, set the timer
        // wait for them to acknowledge the transition
        if (transition_count > 0) {
            hierarchy->timing->schedule_event(trans_timeout_event, trans_timeout,
                reinterpret_cast<std::uint64_t>(this));
        } else {
            complete_members_transition();
        }
    }

    /*! \brief Transtition the children domains's state
     * 
     * The function iterates through all children of the domain. The children
     * are a linked node to each other through the peer. Each child we iterate,
     * we request a domain transition for it.
     *
     * If there is no children, that means we should either complete the domain transition
     * or finish with doing member transitions.
    */
    void domain::do_children_transition() {
        domain_ptr next = child;
        std::uint32_t ccount = child_count;

        /* Iterate through all children of the domains. The next children is linked with the current children
            through the peer (linked list). We will keep iterate until running out of children.
        */
        if (child_count > 0) {
            do {
                next->do_domain_transition();
                next = next->peer;
            } while (next);
        } else {
            // If there is no children, it means that we have reached the end of the hierarchy tree. Call complete to
            // switch to member transition or complete the domain transition.
            complete_children_transition();
        }
    }

    /*! \brief Do the domain transition 
        *
        * The transition will either do the transition for its child first, or transition the state 
        * for all the member sessions attached to it first. This priority is defined by the traverse 
        * direction
    */
    void domain::do_domain_transition() {
        if (hierarchy->traverse_dir == ETraverseChildrenFirst) {
            do_children_transition();
            return;
        }

        do_members_transition();
    }

    void domain::complete_members_transition() {
        // Traverse children first traverse through all childs (child domains),
        // then traverse to members (sessions attached) to do transition. If this is called, it
        // must have been that the transition is done, since this was called
        // after members transitions were done.
        if (hierarchy->traverse_dir == ETraverseChildrenFirst) {
            complete_domain_transition();
        } else {
            do_members_transition();
        }
    }

    void domain::complete_children_transition() {
        // Children are traverse first, so after children transition were done,
        // member transitions are next.
        if (hierarchy->traverse_dir == ETraverseChildrenFirst) {
            do_members_transition();
        } else {
            complete_domain_transition();
        }
    }

    void domain::complete_domain_transition() {
        if (&(*hierarchy->trans_domain) == this) {
            const int err = hierarchy->transitions_fail.size() > 0
                ? hierarchy->transitions_fail[0].err
                : KErrNone;

            cancel_transition();
            hierarchy->finish_trans_request(err);
        } else {
            if (!--parent->transition_count) {
                parent->complete_children_transition();
            }
        }
    }

    void domain::complete_acknowledge_with_err(const int err) {
        // If not fine, not KErrNone, it should be fail domain transition
        if (err) {
            hierarchy->add_transition_failure(id, err);

            if (observed) {
                if (hierarchy->observe_type & EDmNotifyFail) {
                    hierarchy->add_transition(id, get_previous_state(), err);

                    if (hierarchy->is_observe_nof_outstanding()) {
                        hierarchy->finish_observe_request(KErrNone);
                    }
                }
            }

            if (hierarchy->fail_policy == ETransitionFailureStop) {
                hierarchy->finish_trans_request(err);
                return;
            }
        } else {
            if (observed) {
                if (hierarchy->observe_type & EDmNotifyPass) {
                    hierarchy->add_transition(id, get_previous_state(), err);

                    if (hierarchy->is_observe_nof_outstanding()) {
                        hierarchy->finish_observe_request(KErrNone);
                    }
                }
            }
        }

        if (!--transition_count) {
            hierarchy->timing->unschedule_event(trans_timeout_event, reinterpret_cast<std::uint64_t>(this));
            complete_members_transition();
        }
    }

    void domainmngr_server::request_domain_transition(service::ipc_context ctx) {
        const hierarchy_ptr target_hier = control_hierarchies[ctx.msg->msg_session->unique_id()];

        if (!target_hier) {
            ctx.set_request_status(KErrBadHierarchyId);
            return;
        }

        const std::uint16_t domain_id = static_cast<std::uint16_t>(*ctx.get_arg<int>(0));
        const std::int32_t target_state = static_cast<int32_t>(1);
        const TDmTraverseDirection dir = static_cast<TDmTraverseDirection>(2);

        bool res = target_hier->transition(ctx.msg->request_sts, domain_id, target_state, dir);

        if (!res) {
            ctx.set_request_status(KDmErrBadDomainId);
            return;
        }

        ctx.set_request_status(KErrNone);
    }

    void domainmngr_server::request_system_transition(service::ipc_context ctx) {
        const hierarchy_ptr target_hier = control_hierarchies[ctx.msg->msg_session->unique_id()];

        if (!target_hier) {
            ctx.set_request_status(KErrBadHierarchyId);
            return;
        }

        const std::int32_t target_state = static_cast<int32_t>(0);
        const TDmTraverseDirection dir = static_cast<TDmTraverseDirection>(1);

        bool res = target_hier->transition(ctx.msg->request_sts, 0, target_state, dir);

        if (!res) {
            ctx.set_request_status(KDmErrBadDomainId);
            return;
        }

        ctx.set_request_status(KErrNone);
    }

    void domainmngr_server::cancel_transition(service::ipc_context ctx) {
        const hierarchy_ptr target_hier = control_hierarchies[ctx.msg->msg_session->unique_id()];

        if (!target_hier) {
            ctx.set_request_status(KErrBadHierarchyId);
            return;
        }

        memory_system *mem = ctx.sys->get_memory_system();

        if (target_hier->trans_status) {
            *(target_hier->trans_status.get(target_hier->trans_status_thr->owning_process())) = KErrCancel;
            target_hier->trans_status = 0;
            target_hier->trans_status_thr = nullptr;
        }

        if (target_hier->observe_status && target_hier->observer_started) {
            *(target_hier->observe_status.get(target_hier->obs_status_thr->owning_process())) = KErrCancel;
            target_hier->observe_status = 0;
            target_hier->obs_status_thr = nullptr;
        }

        if (target_hier->trans_domain) {
            target_hier->trans_domain->observed = false;
        }

        ctx.set_request_status(KErrNone);
    }

    void domainmngr_server::get_transition_fail_count(service::ipc_context ctx) {
        const hierarchy_ptr target_hier = control_hierarchies[ctx.msg->msg_session->unique_id()];

        if (!target_hier) {
            ctx.set_request_status(KErrBadHierarchyId);
            return;
        }

        ctx.set_request_status(static_cast<int>(target_hier->transitions_fail.size()));
    }

    void domainmngr_server::observer_join(service::ipc_context ctx) {
        const hierarchy_ptr target_hier = control_hierarchies[ctx.msg->msg_session->unique_id()];

        if (!target_hier) {
            ctx.set_request_status(KErrBadHierarchyId);
            return;
        }

        if (target_hier->observe_session) {
            ctx.set_request_status(KDmErrBadSequence);
            return;
        }

        target_hier->observe_session = ctx.msg->msg_session;
        target_hier->transitions.clear();

        ctx.set_request_status(KErrNone);
    }

    void domainmngr_server::observer_start(service::ipc_context ctx) {
        const hierarchy_ptr target_hier = control_hierarchies[ctx.msg->msg_session->unique_id()];

        if (!target_hier) {
            ctx.set_request_status(KErrBadHierarchyId);
            return;
        }

        if (target_hier->observe_session != ctx.msg->msg_session || target_hier->observer_started) {
            ctx.set_request_status(KDmErrBadSequence);
            return;
        }

        target_hier->observer_started = true;
        target_hier->observe_type = *ctx.get_arg<int>(1);
        const std::uint16_t dm_id = *ctx.get_arg<int>(0);

        domain_ptr dm = target_hier->lookup(dm_id);

        if (!dm) {
            ctx.set_request_status(KDmErrBadDomainId);
            return;
        }

        dm->set_observe(true);
        target_hier->observed_domain = dm;

        ctx.set_request_status(KErrNone);
    }

    void domainmngr_server::observer_cancel(service::ipc_context ctx) {
        const hierarchy_ptr target_hier = control_hierarchies[ctx.msg->msg_session->unique_id()];

        if (!target_hier) {
            ctx.set_request_status(KErrBadHierarchyId);
            return;
        }

        if (!target_hier->observe_session) {
            ctx.set_request_status(KDmErrBadSequence);
            return;
        }

        if (target_hier->observer_started) {
            target_hier->observer_started = false;
            target_hier->observed_domain->set_observe(false);

            target_hier->observed_domain = nullptr;
        }

        ctx.set_request_status(KErrNone);
    }

    void domainmngr_server::observer_notify(service::ipc_context ctx) {
        const hierarchy_ptr target_hier = control_hierarchies[ctx.msg->msg_session->unique_id()];

        if (!target_hier) {
            ctx.set_request_status(KErrBadHierarchyId);
            return;
        }

        if (target_hier->observe_session != ctx.msg->msg_session || !target_hier->observer_started) {
            ctx.set_request_status(KDmErrBadSequence);
            return;
        }

        target_hier->deferral_statuses.emplace(ctx.msg->msg_session->unique_id(),
            std::make_pair(ctx.msg->request_sts, ctx.msg->own_thr));
    }

    void domainmngr_server::observed_count(service::ipc_context ctx) {
        const hierarchy_ptr target_hier = control_hierarchies[ctx.msg->msg_session->unique_id()];

        if (!target_hier) {
            ctx.set_request_status(KErrBadHierarchyId);
            return;
        }

        if (target_hier->observe_session != ctx.msg->msg_session || !target_hier->observer_started) {
            ctx.set_request_status(KDmErrBadSequence);
            return;
        }

        ctx.set_request_status(target_hier->observed_children);
    }

    domain_server::domain_server(eka2l1::system *sys, std::shared_ptr<domain_manager> &mngr)
        : server(sys, "!DmDomainServer", true)
        , mngr(mngr) {
        /* REGISTER IPC */
        REGISTER_IPC(domain_server, join_domain, EDmDomainJoin, "DmDomain::JoinDomain");
        REGISTER_IPC(domain_server, request_transition_nof, EDmStateRequestTransitionNotification, "DmDomain::ReqTransNof");
        REGISTER_IPC(domain_server, cancel_transition_nof, EDmStateCancelTransitionNotification, "DmDomain::CancelTransNof");
        REGISTER_IPC(domain_server, acknowledge_last_state, EDmStateAcknowledge, "DmDomain::AcknowledgeLastState");
        REGISTER_IPC(domain_server, defer_acknowledge, EDmStateDeferAcknowledgement, "DmDomain::DeferAcknowledge");
        REGISTER_IPC(domain_server, cancel_defer_acknowledge, EDmStateDeferAcknowledgement, "DmDomain::CancelDeferAcknowledge");
    }

    domainmngr_server::domainmngr_server(eka2l1::system *sys)
        : server(sys, "!DmManagerServer", true) {
        mngr = std::make_shared<domain_manager>();
        mngr->timing = sys->get_timing_system();
        mngr->kern = sys->get_kernel_system();

        property_ptr init_prop = kern->create<service::property>();

        init_prop->first = dm_category;
        init_prop->second = dm_init_key;

        init_prop->define(service::property_type::int_data, 0);

        init_prop->set_int(1);

        /* REGISTER IPC */
        REGISTER_IPC(domainmngr_server, add_new_hierarchy, EDmHierarchyAdd, "DmManager::AddHierarchy");
        REGISTER_IPC(domainmngr_server, join_hierarchy, EDmHierarchyJoin, "DmManager::JoinHierarchy");
        REGISTER_IPC(domainmngr_server, request_domain_transition, EDmRequestDomainTransition, "DmManager::ReqDomainTrans");
        REGISTER_IPC(domainmngr_server, request_system_transition, EDmRequestSystemTransition, "DmManager::ReqSystemTrans");
        REGISTER_IPC(domainmngr_server, cancel_transition, EDmCancelTransition, "DmManager::CancelTrans");
        REGISTER_IPC(domainmngr_server, get_transition_fail_count, EDmGetTransitionFailureCount, "DmManager::GetTransitionFailureCount");
        REGISTER_IPC(domainmngr_server, observer_join, EDmObserverJoin, "DmManager::ObserverJoin");
        REGISTER_IPC(domainmngr_server, observer_start, EDmObserverStart, "DmManager::ObserverStart");
        REGISTER_IPC(domainmngr_server, observer_cancel, EDmObserverCancel, "DmManager::ObserverCancel");
        REGISTER_IPC(domainmngr_server, observer_notify, EDmObserverNotify, "DmManager::ObserverNotify");
        REGISTER_IPC(domainmngr_server, observed_count, EDmObserveredCount, "DmManager::ObservedCount");
    }
}