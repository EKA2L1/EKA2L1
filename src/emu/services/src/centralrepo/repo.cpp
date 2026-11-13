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
#include <kernel/kernel.h>

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace eka2l1 {
    std::uint32_t central_repo::get_default_meta_for_new_key(const std::uint32_t key) {
        for (std::size_t i = 0; i < meta_range.size(); i++) {
            // Normal range
            if ((meta_range[i].low_key <= key) && (key <= meta_range[i].high_key)) {
                return meta_range[i].default_meta_data;
            }

            // Or a partial key: the low key is the pattern the masked key must match.
            if (meta_range[i].key_mask && ((meta_range[i].key_mask & key) == meta_range[i].low_key)) {
                return meta_range[i].default_meta_data;
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

    void central_repo_client_subsession::modification_success(const std::uint32_t key, const bool committing) {
        for (auto *session : attach_repo->attached) {
            if (session != this && session->is_active() && !session->transaction_error
                && (committing || session->transaction_mode == central_repo_transaction_mode::concurrent_read_write)) {
                session->transaction_error = epoc::error_locked;
            }

            common::erase_elements(session->notifies, [key, this](cenrep_notify_info &notify) {
                if ((key & notify.mask) != (notify.match & notify.mask)) {
                    return false;
                }
                if (server->get_kernel_object_owner()->is_thread_alive(notify.sts.requester)) {
                    notify.sts.complete(static_cast<std::int32_t>(key));
                }
                return true;
            });
        }
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

    central_repo_entry *central_repo_client_subsession::get_entry(const std::uint32_t key) {
        const auto entry = transaction_entries.find(key);
        return (entry != transaction_entries.end()) ? &entry->second : attach_repo->find_entry(key);
    }

    int central_repo_client_subsession::prepare_write() {
        using mode = central_repo_transaction_mode;
        if (transaction_error) {
            return epoc::error_abort;
        }
        for (const auto *session : attach_repo->attached) {
            if (session == this || session->transaction_error) {
                continue;
            }
            if (session->transaction_mode == mode::read_write
                || ((transaction_mode != mode::concurrent_read_write) && session->transaction_mode == mode::read)) {
                return epoc::error_locked;
            }
        }
        if (transaction_mode == mode::read) {
            transaction_mode = mode::read_write;
        }
        return epoc::error_none;
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

    static std::optional<central_repo_entry_variant> read_setting(service::ipc_context *ctx) {
        central_repo_entry_variant value{};
        switch (ctx->msg->function) {
        case cen_rep_create_int:
        case cen_rep_set_int:
            value.etype = central_repo_entry_type::integer;
            value.intd = ctx->msg->args.args[1];
            break;

        case cen_rep_create_real:
        case cen_rep_set_real: {
            const auto real = ctx->get_argument_data_from_descriptor<double>(1);
            if (!real || ctx->get_argument_data_size(1) != sizeof(double)) {
                return std::nullopt;
            }
            value.etype = central_repo_entry_type::real;
            value.reald = *real;
            break;
        }

        default: {
            auto string = ctx->get_argument_value<std::string>(1);
            if (!string) {
                return std::nullopt;
            }
            value.etype = central_repo_entry_type::string;
            value.strd = std::move(*string);
            break;
        }
        }
        return value;
    }

    static bool same_setting(const central_repo_entry_variant &a, const central_repo_entry_variant &b) {
        if (a.etype != b.etype) {
            return false;
        }
        switch (a.etype) {
        case central_repo_entry_type::integer:
            return a.intd == b.intd;
        case central_repo_entry_type::real:
            return a.reald == b.reald;
        case central_repo_entry_type::string:
            return a.strd == b.strd;
        default:
            return false;
        }
    }

    void central_repo_client_subsession::set_entry(service::ipc_context *ctx, const bool create) {
        const std::uint32_t key = ctx->msg->args.args[0];
        int error = prepare_write();
        if (!error) {
            auto value = read_setting(ctx);
            const auto *existing = get_entry(key);
            if (create && existing) {
                error = epoc::error_already_exists;
            } else if (!value || (existing && existing->data.etype != value->etype)) {
                error = epoc::error_argument;
            } else if (!existing || !same_setting(existing->data, *value)) {
                if (is_active()) {
                    central_repo_entry entry = existing ? *existing : central_repo_entry{};
                    entry.key = key;
                    if (!existing) {
                        entry.metadata_val = attach_repo->get_default_meta_for_new_key(key);
                    }
                    entry.data = std::move(*value);
                    transaction_entries.insert_or_assign(key, std::move(entry));
                } else {
                    if (existing) {
                        attach_repo->find_entry(key)->data = std::move(*value);
                    } else {
                        attach_repo->add_new_entry(key, *value);
                    }
                    modification_success(key);
                }
            }
        }
        if (error && is_active() && !transaction_error) {
            transaction_error = error;
            transaction_error_key = key;
        }
        ctx->complete(error);
    }

    void central_repo_client_subsession::create_value(service::ipc_context *ctx) {
        set_entry(ctx, true);
    }

    void central_repo_client_subsession::set_value(service::ipc_context *ctx) {
        set_entry(ctx, false);
    }

#ifdef _MSC_VER
#pragma optimize("", off)
#endif
    void central_repo_client_subsession::get_value(service::ipc_context *ctx) {
        if (transaction_error) {
            ctx->complete(epoc::error_abort);
            return;
        }
        // We get the entry.
        // Use mode 0 (write) to get the entry, since we are modifying data.
        std::optional<std::uint32_t> the_key = ctx->get_argument_value<std::uint32_t>(0);

        if (!the_key.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        central_repo_entry *entry = get_entry(the_key.value());

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

    void central_repo_client_subsession::find(service::ipc_context *ctx) {
        key_found_result.clear();
        if (transaction_error) {
            ctx->complete(epoc::error_abort);
            return;
        }

        // The 9.1 client passes raw key and mask; the later client packages them in slot 0.
        const bool plain_filter = !(static_cast<int>(ctx->msg->args.get_arg_type(0))
            & static_cast<int>(ipc_arg_type::flag_des));
        const int value_slot = plain_filter ? 2 : 1;
        const int result_slot = plain_filter ? 3 : 2;
        const auto filter = plain_filter
            ? std::optional<central_repo_key_filter>{{ static_cast<std::uint32_t>(ctx->msg->args.args[0]),
                static_cast<std::uint32_t>(ctx->msg->args.args[1]) }}
            : ctx->get_argument_data_from_descriptor<central_repo_key_filter>(0);
        const auto capacity = ctx->get_argument_max_data_size(result_slot) / sizeof(std::uint32_t);
        if (!filter || (!plain_filter && ctx->get_argument_data_size(0) != sizeof(central_repo_key_filter))
            || !capacity || ctx->msg->args.get_arg_type(result_slot) != ipc_arg_type::des8) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::optional<central_repo_entry_variant> value;
        const auto function = ctx->msg->function;
        const bool not_equal = function >= cen_rep_find_neq_int && function <= cen_rep_find_neq_string;
        if (function != cen_rep_find) {
            central_repo_entry_variant expected{};
            switch (function) {
            case cen_rep_find_eq_int:
            case cen_rep_find_neq_int:
                expected.etype = central_repo_entry_type::integer;
                expected.intd = ctx->msg->args.args[value_slot];
                value = expected;
                break;
            case cen_rep_find_eq_real:
            case cen_rep_find_neq_real:
                if (const auto real = ctx->get_argument_data_from_descriptor<double>(value_slot);
                    real && ctx->get_argument_data_size(value_slot) == sizeof(double)) {
                    expected.etype = central_repo_entry_type::real;
                    expected.reald = *real;
                    value = expected;
                }
                break;
            default:
                if (auto str = ctx->get_argument_value<std::string>(value_slot)) {
                    expected.etype = central_repo_entry_type::string;
                    expected.strd = std::move(*str);
                    value = std::move(expected);
                }
                break;
            }
            if (!value) {
                ctx->complete(epoc::error_argument);
                return;
            }
        }

        std::vector<std::uint32_t> result{0};
        const auto append = [&](const central_repo_entry &entry) {
            if ((entry.key & filter->id_mask) == (filter->partial_key & filter->id_mask)
                && (!value || (entry.data.etype == value->etype && same_setting(entry.data, *value) != not_equal))) {
                ++result[0];
                if (result.size() < capacity) {
                    result.push_back(entry.key);
                } else {
                    key_found_result.push_back(entry.key);
                }
            }
        };
        for (const auto &entry : attach_repo->entries) {
            if (transaction_entries.find(entry.key) == transaction_entries.end()) {
                append(entry);
            }
        }
        for (const auto &[key, entry] : transaction_entries) {
            append(entry);
        }

        const bool written = ctx->write_data_to_descriptor_argument(result_slot,
            reinterpret_cast<const std::uint8_t *>(result.data()), static_cast<std::uint32_t>(result.size() * sizeof(std::uint32_t)));
        ctx->complete(!written ? epoc::error_bad_descriptor : (result[0] ? epoc::error_none : epoc::error_not_found));
    }

    void central_repo_client_subsession::get_find_result(service::ipc_context *ctx) {
        const auto capacity = ctx->get_argument_max_data_size(0) / sizeof(std::uint32_t);
        const auto count = std::min(capacity, key_found_result.size());
        const bool written = ctx->write_data_to_descriptor_argument(0,
            reinterpret_cast<const std::uint8_t *>(key_found_result.data()), static_cast<std::uint32_t>(count * sizeof(std::uint32_t)));
        ctx->complete(written ? epoc::error_none : epoc::error_bad_descriptor);
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
        using mode = central_repo_transaction_mode;
        const auto requested = ctx->msg->args.args[0];
        if (is_active() || requested < static_cast<unsigned>(mode::read)
            || requested > static_cast<unsigned>(mode::read_write)) {
            ctx->complete(epoc::error_argument);
            return;
        }

        for (const auto *session : attach_repo->attached) {
            if (!session->transaction_error && requested != static_cast<unsigned>(mode::concurrent_read_write)
                && (session->transaction_mode == mode::read_write
                    || (requested == static_cast<unsigned>(mode::read_write) && session->transaction_mode == mode::read))) {
                ctx->complete(epoc::error_locked);
                return;
            }
        }
        transaction_mode = static_cast<mode>(requested);
        ctx->complete(epoc::error_none);
    }

    void central_repo_client_subsession::clear_transaction() {
        transaction_mode = central_repo_transaction_mode::none;
        transaction_entries.clear();
        transaction_error = 0;
        transaction_error_key = 0xFFFFFFFF;
    }

    void central_repo_client_subsession::commit_transaction(service::ipc_context *ctx) {
        if (!is_active()) {
            ctx->complete(epoc::error_not_ready);
            return;
        }

        if (ctx->msg->args.get_arg_type(0) != ipc_arg_type::des8
            || !ctx->get_descriptor_argument_ptr(0) || ctx->get_argument_max_data_size(0) < sizeof(std::uint32_t)) {
            clear_transaction();
            ctx->complete(epoc::error_bad_descriptor);
            return;
        }

        int error = transaction_error;
        std::uint32_t key_info = transaction_error_key;
        std::vector<std::uint32_t> changed;
        if (!error && !transaction_entries.empty()) {
            auto previous = attach_repo->entries;
            for (auto &[key, entry] : transaction_entries) {
                if (auto *current = attach_repo->find_entry(key)) {
                    if (same_setting(current->data, entry.data)) {
                        continue;
                    }
                    *current = std::move(entry);
                } else {
                    attach_repo->entries.push_back(std::move(entry));
                }
                changed.push_back(key);
            }

            if (!changed.empty() && !attach_repo->write_changes(ctx->sys->get_io_system(), ctx->sys->get_device_manager(), true)) {
                attach_repo->entries = std::move(previous);
                error = epoc::error_write;
            }
        }

        clear_transaction();
        if (!error) {
            key_info = static_cast<std::uint32_t>(changed.size());
            for (const auto key : changed) {
                modification_success(key, true);
            }
        }
        ctx->write_data_to_descriptor_argument<std::uint32_t>(0, key_info);
        ctx->complete(error);
    }

    void central_repo_client_subsession::cancel_transaction(service::ipc_context *ctx) {
        clear_transaction();
        ctx->complete(epoc::error_none);
    }
}
