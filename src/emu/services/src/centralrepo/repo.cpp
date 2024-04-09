/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project.
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

#include <services/centralrepo/centralrepo.h>
#include <services/centralrepo/repo.h>
#include <services/context.h>
#include <system/epoc.h>
#include <utils/err.h>

#include <common/cvt.h>
#include <system/devices.h>

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace eka2l1 {
    std::uint32_t central_repo::get_default_meta_for_new_key(const std::uint32_t key) {
        for (std::size_t i = 0; i < meta_range.size(); i++) {
            if (meta_range[i].high_key) {
                // Normal range
                if (meta_range[i].low_key <= key
                    && key <= meta_range[i].high_key) {
                    return meta_range[i].default_meta_data;
                }
            } else {
                if ((meta_range[i].key_mask & key)
                    == (meta_range[i].low_key & key)) {
                    return meta_range[i].default_meta_data;
                }
            }
        }

        return default_meta;
    }

    bool central_repo::add_new_entry(const std::uint32_t key, const central_repo_entry_variant &var) {
        if (find_entry(key)) {
            return false;
        }

        // Find default meta
        central_repo_entry entry;
        entry.metadata_val = get_default_meta_for_new_key(key);
        entry.key = key;
        entry.data = var;

        entries.push_back(entry);

        return true;
    }

    bool central_repo::add_new_entry(const std::uint32_t key, const central_repo_entry_variant &var,
        const std::uint32_t meta) {
        if (find_entry(key)) {
            return false;
        }

        // Find default meta
        central_repo_entry entry;
        entry.metadata_val = meta;
        entry.key = key;
        entry.data = var;

        entries.push_back(entry);

        return true;
    }

    central_repo_entry *central_repo::find_entry(const std::uint32_t key) {
        auto ite = std::find_if(entries.begin(), entries.end(), [=](const central_repo_entry &e) {
            return e.key == key;
        });

        if (ite == entries.end()) {
            return nullptr;
        }

        return &(*ite);
    }

    void central_repo::query_entries(const std::uint32_t partial_key, const std::uint32_t mask,
        std::vector<central_repo_entry *> &matched_entries,
        const central_repo_entry_type etype) {
        std::uint32_t required_mask = mask & partial_key;

        for (auto &entry : entries) {
            if ((entry.key & required_mask) && (entry.data.etype == etype)) {
                matched_entries.push_back(&entry);
            }
        }
    }

    central_repo_client_subsession::central_repo_client_subsession()
        : flags(0) {
        set_transaction_mode(central_repo_transaction_mode::read_write);
    }

    void central_repo_client_subsession::modification_success(const std::uint32_t key) {
        // Iters through all
        for (std::size_t i = 0; i < notifies.size(); i++) {
            cenrep_notify_info &notify = notifies[i];

            if ((key & (notify.match & notify.mask))) {
                // Notify and delete this request from the list
                notify.sts.complete(0);
                notifies.erase(notifies.begin() + i);
            }
        }

        // Done all requests
    }

    int central_repo_client_subsession::add_notify_request(epoc::notify_info &info,
        const std::uint32_t mask, const std::uint32_t match) {
        auto find_result = std::find_if(notifies.begin(), notifies.end(), [&](const cenrep_notify_info &notify) {
            return (notify.mask == mask) && (notify.match == match);
        });

        if (find_result != notifies.end()) {
            return -1;
        } else {
            if (info.empty()) {
                // Likely a test, pass
                return 0;
            }
        }

        notifies.push_back({ info, mask, match });
        return 0;
    }

    void central_repo_client_subsession::cancel_all_notify_requests() {
        common::erase_elements(notifies, [=](cenrep_notify_info &info) {
            info.sts.complete(-3);
            return true;
        });
    }

    void central_repo_client_subsession::cancel_notify_request(const std::uint32_t match_key, const std::uint32_t mask) {
        common::erase_elements(notifies, [=](cenrep_notify_info &info) {
            if ((info.match == match_key) && (info.mask == mask)) {
                info.sts.complete(-3);
                return true;
            }

            return false;
        });
    }

    central_repo_entry *central_repo_client_subsession::get_entry(const std::uint32_t key, int mode) {
        // Repo is in transaction
        bool active = is_active();

        // Resolve
        // If get entry for write but the transaction mode is read only, we can't allow that
        if ((mode == 1) && (get_transaction_mode() == central_repo_transaction_mode::read_only)) {
            return nullptr;
        }

        // Check transactor, only if its in transaction
        if (active) {
            auto entry_ite = transactor.changes.find(key);
            if (entry_ite != transactor.changes.end()) {
                return &(entry_ite->second);
            }
        }

        // If not in transaction, or if we are in transaction but read-mode
        // Directly get the repo data
        if (!active || mode == 0) {
            auto result = std::find_if(attach_repo->entries.begin(), attach_repo->entries.end(),
                [&](const central_repo_entry &entry) { return entry.key == key; });

            if (result == attach_repo->entries.end()) {
                return nullptr;
            }

            return &(*result);
        }

        transactor.changes.emplace(key, central_repo_entry{});
        return &(transactor.changes[key]);
    }

    bool central_repos_cacher::free_oldest() {
        std::uint32_t repo_key = 0xFFFFFFFF;
        std::uint64_t oldest_access = 0xFFFFFFFFFFFFFFFF;

        for (auto &[key, entry] : entries) {
            if (entry.last_access < oldest_access && entry.repo.access_count == 0) {
                oldest_access = entry.last_access;
                repo_key = key;
            }
        }

        if (repo_key != 0xFFFFFFFF) {
            entries.erase(repo_key);
            return true;
        }

        return false;
    }

    eka2l1::central_repo *central_repos_cacher::add_repo(const std::uint32_t key, eka2l1::central_repo &repo) {
        if (entries.size() == MAX_REPO_CACHE_ENTRIES) {
            // Free the oldest
            if (!free_oldest()) {
                return nullptr;
            }
        }

        cache_entry entry;
        entry.last_access = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                                .count();

        entry.repo = std::move(repo);
        entry.repo.access_count = 1;

        auto res = entries.emplace(key, std::move(entry));

        if (res.second) {
            return &res.first->second.repo;
        }

        return nullptr;
    }

    bool central_repos_cacher::remove_repo(const std::uint32_t key) {
        if (entries.erase(key)) {
            return true;
        }

        return false;
    }

    eka2l1::central_repo *central_repos_cacher::get_cached_repo(const std::uint32_t key) {
        auto ite = entries.find(key);

        if (ite == entries.end()) {
            return nullptr;
        }

        ite->second.last_access = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                                      .count();
        ite->second.repo.access_count++;

        return &(ite->second.repo);
    }

    void central_repo_client_subsession::reset(service::ipc_context *ctx) {
        io_system *io = ctx->sys->get_io_system();
        device_manager *mngr = ctx->sys->get_device_manager();

        eka2l1::central_repo *init_repo = server->get_initial_repo(io, mngr, attach_repo->uid);

        // Reset the keys
        const std::uint32_t key = *ctx->get_argument_value<std::uint32_t>(0);
        int err = reset_key(init_repo, key);

        // In transaction
        if (err == -1) {
            ctx->complete(epoc::error_not_supported);
            return;
        }

        if (err == -2) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        // Write committed changes to disk
        write_changes(io, mngr);
        modification_success(key);

        ctx->complete(epoc::error_none);
    }

    void central_repo_client_subsession::create_value(service::ipc_context *ctx) {
        std::optional<std::uint32_t> id = ctx->get_argument_value<std::uint32_t>(0);

        if (!id) {
            ctx->complete(epoc::error_argument);
            return;
        }

        central_repo_entry_variant new_var;
        bool hit = true;

        switch (ctx->msg->function) {
        case cen_rep_create_int:
            new_var.etype = central_repo_entry_type::integer;
            new_var.intd = ctx->msg->args.args[1];

            break;

        case cen_rep_create_real: {
            new_var.etype = central_repo_entry_type::real;
            std::optional<double> dd_val = ctx->get_argument_data_from_descriptor<double>(1);

            if (!dd_val.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            new_var.reald = dd_val.value();
            break;
        }

        case cen_rep_create_string: {
            new_var.etype = central_repo_entry_type::string;
            std::optional<std::string> bb_val = ctx->get_argument_value<std::string>(1);

            if (!bb_val.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            new_var.strd.resize(bb_val->length());
            std::memcpy(new_var.strd.data(), bb_val->data(), bb_val->size());

            break;
        }

        default:
            hit = false;
            break;
        }

        if (!hit) {
            LOG_ERROR(SERVICE_CENREP, "Unknown create value type!");
            ctx->complete(epoc::error_general);
            return;
        }

        if (!attach_repo->add_new_entry(id.value(), new_var)) {
            LOG_ERROR(SERVICE_CENREP, "Fail to add new entry with id {}", id.value());
            ctx->complete(epoc::error_general);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    void central_repo_client_subsession::set_value(service::ipc_context *ctx) {
        // We get the entry.
        // Use mode 1 (write) to get the entry, since we are modifying data.
        central_repo_entry *entry = get_entry(*ctx->get_argument_value<std::uint32_t>(0), 1);

        // If it does not exist, or it is in different type, discard.
        // Depends on the invalid type, we set error code
        if (!entry) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        // TODO: Capability supply (+Policy)
        // This is really bad... We are not really care about accuracy right now
        // Assuming programs did right things, and accept the rules
        switch (ctx->msg->function) {
        case cen_rep_set_int: {
            if (entry->data.etype != central_repo_entry_type::integer) {
                ctx->complete(epoc::error_argument);
                break;
            }

            entry->data.intd = static_cast<std::uint64_t>(*ctx->get_argument_value<std::uint32_t>(1));
            break;
        }

        case cen_rep_set_real: {
            if (entry->data.etype != central_repo_entry_type::real) {
                ctx->complete(epoc::error_argument);
                break;
            }

            std::optional<double> data = ctx->get_argument_data_from_descriptor<double>(1);
            if (!data.has_value()) {
                ctx->complete(epoc::error_argument);
                break;
            }

            entry->data.reald = data.value();
            break;
        }

        case cen_rep_set_string: {
            if (entry->data.etype != central_repo_entry_type::string) {
                ctx->complete(epoc::error_argument);
                break;
            }

            entry->data.strd = *ctx->get_argument_value<std::string>(1);
            break;
        }

        default: {
            // Unreachable
            assert(false);
            break;
        }
        }

        // Success in modifying
        modification_success(entry->key);
        ctx->complete(epoc::error_none);
    }

#ifdef _MSC_VER
#pragma optimize("", off)
#endif
    void central_repo_client_subsession::get_value(service::ipc_context *ctx) {
        // We get the entry.
        // Use mode 0 (write) to get the entry, since we are modifying data.
        std::optional<std::uint32_t> the_key = ctx->get_argument_value<std::uint32_t>(0);

        if (!the_key.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        central_repo_entry *entry = get_entry(the_key.value(), 0);

        if (!entry) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        switch (ctx->msg->function) {
        case cen_rep_get_int: {
            if (entry->data.etype != central_repo_entry_type::integer) {
                ctx->complete(epoc::error_argument);
                return;
            }

            const std::uint32_t result_int = static_cast<std::uint32_t>(entry->data.intd);
            ctx->write_data_to_descriptor_argument<std::uint32_t>(1, result_int);

            break;
        }

        case cen_rep_get_real: {
            if (entry->data.etype != central_repo_entry_type::real) {
                ctx->complete(epoc::error_argument);
                return;
            }

            const double result_fl = static_cast<double>(entry->data.reald);
            ctx->write_data_to_descriptor_argument<double>(1, result_fl);

            break;
        }

        case cen_rep_get_string: {
            if (entry->data.etype == central_repo_entry_type::string) {
                std::optional<std::uint32_t> wanted_length = ctx->get_argument_data_from_descriptor<std::uint32_t>(2);
                if (wanted_length.has_value()) {
                    wanted_length.value() = static_cast<std::uint32_t>(entry->data.strd.length());
                    ctx->write_data_to_descriptor_argument<std::uint32_t>(2, wanted_length.value());
                }

                const std::size_t buffer_length = ctx->get_argument_max_data_size(1);

                ctx->write_data_to_descriptor_argument(1, reinterpret_cast<std::uint8_t *>(&entry->data.strd[0]),
                    static_cast<std::uint32_t>(common::min(entry->data.strd.length(), buffer_length)));

                if (buffer_length < entry->data.strd.length()) {
                    ctx->complete(epoc::error_overflow);
                    return;
                }
            } else {
                ctx->complete(epoc::error_argument);
                return;
            }

            break;
        }

        default:
            LOG_ERROR(SERVICE_CENREP, "Invalid cenrep entry get type!");
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->complete(epoc::error_none);
    }
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

    void central_repo_client_subsession::append_new_key_to_found_eq_list(std::uint32_t *array, const std::uint32_t key, const std::uint32_t max_uids_buf) {
        // We have to push it to the temporary array, since this array can be retrieve anytime before another FindEq call
        // Even if the provided array is not full
        array[0]++;

        if (array[0] <= max_uids_buf) {
            // Increase the length, than add the keey
            array[array[0]] = key;
        } else {
            key_found_result.push_back(key);
        }
    }

    void central_repo_client_subsession::find(service::ipc_context *ctx) {
        // Clear found result
        // TODO: Should we?
        key_found_result.clear();

        // Get the filter
        std::optional<central_repo_key_filter> filter = ctx->get_argument_data_from_descriptor<central_repo_key_filter>(0);
        std::uint32_t *found_uid_result_array = reinterpret_cast<std::uint32_t *>(ctx->get_descriptor_argument_ptr(2));
        const std::size_t found_uid_max_uids = (ctx->get_argument_max_data_size(2) / sizeof(std::uint32_t)) - 1;

        if (!filter || !found_uid_result_array) {
            LOG_ERROR(SERVICE_CENREP, "Trying to find equal value in cenrep, but arguments are invalid!");
            ctx->complete(epoc::error_argument);
            return;
        }

        // Set found count to 0
        found_uid_result_array[0] = 0;
        std::string cache_arg;

        for (auto &entry : attach_repo->entries) {
            // Try to match the key first
            if ((entry.key & filter->id_mask) != (filter->partial_key & filter->id_mask)) {
                // Mask doesn't match, abandon this entry
                continue;
            }

            std::uint32_t key_found = 0;
            bool find_not_eq = false;

            switch (ctx->msg->function) {
            case cen_rep_find_neq_int:
            case cen_rep_find_neq_real:
            case cen_rep_find_neq_string:
                find_not_eq = true;
                break;

            default:
                break;
            }

            // Depends on the opcode, we try to match the value
            switch (ctx->msg->function) {
            case cen_rep_find_eq_int:
            case cen_rep_find_neq_int: {
                if (entry.data.etype != central_repo_entry_type::integer) {
                    // It must be integer type
                    break;
                }

                // Index 1 argument contains the value we should look for
                // TODO: Signed/unsigned is dangerous
                if (static_cast<std::int32_t>(entry.data.intd) == *ctx->get_argument_value<std::int32_t>(1)) {
                    if (!find_not_eq) {
                        key_found = entry.key;
                    }
                } else {
                    if (find_not_eq) {
                        key_found = entry.key;
                    }
                }

                break;
            }

            case cen_rep_find_eq_string:
            case cen_rep_find_neq_string: {
                bool is_ok = false;

                if (cache_arg.empty()) {
                    auto ss = ctx->get_argument_value<std::string>(1);

                    if (!ss.has_value()) {
                        ctx->complete(epoc::error_argument);
                        return;
                    }

                    cache_arg = ss.value();
                }

                if (entry.data.etype == central_repo_entry_type::string) {
                    // It must be string type
                    is_ok = (entry.data.strd == cache_arg);
                } else {
                    break;
                }

                if (is_ok) {
                    if (!find_not_eq) {
                        key_found = entry.key;
                    }
                } else {
                    if (find_not_eq) {
                        key_found = entry.key;
                    }
                }
                break;
            }

            case cen_rep_find:
                key_found = entry.key;
                break;

            default: {
                LOG_ERROR(SERVICE_CENREP, "Unimplemented Cenrep's find function for opcode {}", ctx->msg->function);
                break;
            }
            }

            // If we found the key, append it
            if (key_found != 0) {
                append_new_key_to_found_eq_list(found_uid_result_array, key_found, static_cast<std::uint32_t>(found_uid_max_uids));
            }
        }

        if (found_uid_result_array[0] == 0) {
            // NOTHING! NOTHING! omg no
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    void central_repo_client_subsession::get_find_result(service::ipc_context *ctx) {
        std::uint32_t *found_uid_result_array = reinterpret_cast<std::uint32_t *>(ctx->get_descriptor_argument_ptr(0));
        const std::size_t found_uid_max_uids = (ctx->get_argument_max_data_size(0) / sizeof(std::uint32_t));

        ctx->write_data_to_descriptor_argument(0, reinterpret_cast<std::uint8_t *>(&key_found_result[0]), static_cast<std::uint32_t>(common::min(found_uid_max_uids, key_found_result.size()) * sizeof(std::uint32_t)));

        ctx->complete(epoc::error_none);
    }

    void central_repo_client_subsession::notify_nof_check(service::ipc_context *ctx) {
        epoc::notify_info holder;
        holder.sts = 0;

        // Pass a test notify info. Add notify request will not add this, but will check for request existence.
        if (add_notify_request(holder, 0xFFFFFFFF, *ctx->get_argument_value<std::int32_t>(0)) == 0) {
            ctx->complete(epoc::error_none);
            return;
        }

        ctx->complete(epoc::error_already_exists);
    }

    void central_repo_client_subsession::notify(service::ipc_context *ctx) {
        const std::uint32_t mask = (ctx->msg->function == cen_rep_notify_req) ? 0xFFFFFFFF : *ctx->get_argument_value<std::uint32_t>(1);
        const std::uint32_t partial_key = *ctx->get_argument_value<std::uint32_t>(0);

        epoc::notify_info info{ ctx->msg->request_sts, ctx->msg->own_thr };
        const int err = add_notify_request(info, mask, partial_key);

        switch (err) {
        case 0: {
            break;
        }

        case -1: {
            ctx->complete(epoc::error_already_exists);
            break;
        }

        default: {
            LOG_TRACE(SERVICE_CENREP, "Unknown returns code {} from add_notify_request, set status to epoc::error_none", err);
            ctx->complete(epoc::error_none);

            break;
        }
        }
    }

    void central_repo_client_subsession::notify_cancel(service::ipc_context *ctx) {
        std::uint32_t mask = 0;
        std::uint32_t partial_key = 0;

        if (ctx->msg->function == cen_rep_notify_cancel) {
            mask = 0xFFFFFFFF;
            partial_key = *ctx->get_argument_value<std::uint32_t>(0);
        } else {
            std::optional<central_repo_key_filter> filter = ctx->get_argument_data_from_descriptor<central_repo_key_filter>(0);

            if (!filter.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            mask = filter->id_mask;
            partial_key = filter->partial_key;
        }

        cancel_notify_request(partial_key, mask);
        ctx->complete(epoc::error_none);
    }

    int central_repo_client_subsession::reset_key(eka2l1::central_repo *init_repo, const std::uint32_t key) {
        // In transacton, fail
        if (is_active()) {
            return -1;
        }

        central_repo_entry *e = attach_repo->find_entry(key);

        if (!e) {
            return -2;
        }

        central_repo_entry *source_e = init_repo->find_entry(key);

        if (!source_e) {
            return -2;
        }

        e->data = source_e->data;
        e->metadata_val = source_e->metadata_val;

        return 0;
    }

    void central_repo_client_subsession::start_transaction(service::ipc_context *ctx) {
        LOG_TRACE(SERVICE_CENREP, "TransactionStart stubbed");
        ctx->complete(epoc::error_none);
    }

    void central_repo_client_subsession::cancel_transaction(service::ipc_context *ctx) {
        LOG_TRACE(SERVICE_CENREP, "TransactionCancel stubbed");
        ctx->complete(epoc::error_none);
    }

    void central_repo_client_subsession::commit_transaction(service::ipc_context *ctx) {
        LOG_TRACE(SERVICE_CENREP, "TransactionCommit stubbed");
        ctx->complete(epoc::error_none);
    }
}
