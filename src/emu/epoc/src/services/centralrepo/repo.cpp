#include <epoc/services/centralrepo/repo.h>

#include <algorithm>
#include <cstdint>
#include <chrono>

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

    void central_repo_client_session::modification_success(const std::uint32_t key) {
        // Iters through all
        for (std::size_t i = 0; i < notifies.size(); i++) {
            cenrep_notify_info &notify = notifies[i];

            if ((key & notify.mask) == (notify.match & notify.mask)) {
                // Notify and delete this request from the list
                notify.sts.complete(0);
                notifies.erase(notifies.begin() + i);
            }
        }

        // Done all requests
    }

    int central_repo_client_session::add_notify_request(epoc::notify_info &info, 
        const std::uint32_t mask, const std::uint32_t match) {
        auto find_result = std::find_if(notifies.begin(), notifies.end(), [&](const cenrep_notify_info &notify) { 
            return (notify.mask == mask) && (notify.match == match); 
        });

        if (find_result != notifies.end()) {
            return -1;
        }

        notifies.push_back({ std::move(info), mask, match });
        return 0;
    }

    central_repo_entry *central_repo_client_session::get_entry(const std::uint32_t key, int mode) {
        // Repo is in transaction
        bool active = is_active();

        // Resolve
        // If get entry for write but the transaction mode is read only, we can't allow that
        if (mode == 1 && get_transaction_mode() == central_repo_transaction_mode::read_only) {
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

        transactor.changes.emplace(key, central_repo_entry {});
        return &(transactor.changes[key]);
    }

    void central_repos_cacher::free_oldest() {
        std::uint32_t repo_key = 0;
        std::uint64_t oldest_access = 0xFFFFFFFFFFFFFFFF;

        for (auto &[key, entry]: entries) {
            if (entry.last_access < oldest_access) {
                oldest_access = entry.last_access;
                repo_key = key; 
            }
        }

        entries.erase(repo_key);
    }

    eka2l1::central_repo *central_repos_cacher::add_repo(const std::uint32_t key, eka2l1::central_repo &repo) {
        if (entries.size() == MAX_REPO_CACHE_ENTRIES) {
            // Free the oldest
            free_oldest();
        }

        cache_entry entry;
        entry.last_access = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();

        entry.repo = std::move(repo);
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

        return &(ite->second.repo);
    }
}