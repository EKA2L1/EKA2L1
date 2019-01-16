#include <epoc/services/centralrepo/repo.h>

#include <algorithm>
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

    central_repo_entry *central_repo_client_session::get_entry(const std::uint32_t key, int mode) {
        if (!is_active()) {
            return nullptr;
        }

        // Resolve
        // If get entry for write but the transaction mode is read only, we can't allow that
        if (mode == 1 && get_transaction_mode() == central_repo_transaction_mode::read_only) {
            return nullptr;
        }

        // Check transactor
        auto entry_ite = transactor.changes.find(key);
        if (entry_ite != transactor.changes.end()) {
            return &(entry_ite->second);
        }

        if (mode == 0) {
            auto result = std::find_if(attach_repo->entries.begin(), attach_repo->entries.end(),
                [&](const central_repo_entry &entry) { return entry.key == key; });

            if (result == attach_repo->entries.end()) {
                return nullptr;
            }

            return &(*result);
        }

        // Mode write, create new one in transactor
        transactor.changes.emplace(key, central_repo_entry {});
        return &(transactor.changes[key]);
    }
}