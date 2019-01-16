/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#pragma once

#include <epoc/services/centralrepo/common.h>
#include <epoc/utils/sec.h>

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace eka2l1 {
    struct central_repo_entry_variant {
        central_repo_entry_type etype;

        std::uint64_t intd;
        double reald;
        std::string strd;
        std::u16string str16d;
    };

    struct central_repo_entry {
        std::uint32_t key;
        central_repo_entry_variant data;
        std::uint32_t metadata_val;
    };

    class central_repo_client;

    enum class central_repo_transaction_mode : std::uint16_t {
        read_only,
        read_write,
        read_write_async
    };

    struct central_repo_entry_access_policy {
        std::uint32_t low_key;
        std::uint32_t high_key;
        std::uint32_t key_mask;

        epoc::security_policy read_access;
        epoc::security_policy write_access;
    };

    struct central_repo_default_meta {
        std::uint32_t low_key;
        std::uint32_t high_key;
        std::uint32_t key_mask;
        std::uint32_t default_meta_data;
    };

    struct central_repo_client_session;

    struct central_repo {
        // TODO (pent0): Add read/write cap
        std::uint8_t ver;
        std::uint8_t keyspace_type;
        std::uint32_t uid;

        std::uint32_t owner_uid;
        
        std::vector<central_repo_entry> entries;
        std::vector<central_repo_client_session*> attached;

        central_repo_entry_access_policy default_policy;
        std::vector<central_repo_entry_access_policy> single_policies;
        std::vector<central_repo_entry_access_policy> policies_range;

        std::uint32_t default_meta = 0;
        std::vector<central_repo_default_meta> meta_range;

        std::uint64_t time_stamp;

        std::vector<std::uint32_t> deleted_settings;

        central_repo_entry *find_entry(const std::uint32_t key);

        std::uint32_t get_default_meta_for_new_key(const std::uint32_t key);

        bool add_new_entry(const std::uint32_t key, const central_repo_entry_variant &var);
        bool add_new_entry(const std::uint32_t key, const central_repo_entry_variant &var,
            const std::uint32_t meta);
    };
    
    struct central_repo_client_session;

    struct central_repo_transactor {
        std::unordered_map<std::uint32_t, central_repo_entry> changes;
        central_repo_client_session *session;
    };

    struct central_repo_client_session {
        central_repo *attach_repo;
        central_repo_transactor transactor;

        enum session_flags {
            active = 0x1
        };

        std::uint32_t flags;

        bool is_active() const {
            return (flags >> 16) & active;
        }

        void set_active(const bool b) {
            std::uint16_t aflags = flags >> 16;
            flags &= 0x0000FFFF;

            aflags &= ~active;

            if (b) {
                aflags |= active;
            }

            flags |= (aflags << 16);
        }

        void set_transaction_mode(const central_repo_transaction_mode mode) {
            flags &= 0xFFFF0000;
            flags |= static_cast<int>(mode);
        }

        central_repo_transaction_mode get_transaction_mode() {
            return static_cast<central_repo_transaction_mode>(flags);
        }

        /*! \brief Get a pointer to an entry
         *
         * Do the following: 
         * - Check if a transaction is active, otherwise return nullptr
         * - Check if the key is in the transactor entry, and returns
         * - Else, fallback to default
         * 
         * Mode are matters:
         * 0: Read mode: if no entry in transactor, fallback to repo entries
         * 1: Write mode: if no entry, create new
         * 
         * Of course, transaction mode are checked.
         * If we get the entry for write purpose but the transaction mode is read-only, we won't allow that
        */
        central_repo_entry *get_entry(const std::uint32_t key, int mode);
    };
}