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

#pragma once

#include <services/centralrepo/common.h>
#include <utils/reqsts.h>
#include <utils/sec.h>

#include <common/types.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace eka2l1 {
    namespace service {
        struct ipc_context;
    }

    class device_manager;
    class central_repo_server;
    class io_system;

    struct central_repo_entry_variant {
        central_repo_entry_type etype;

        std::uint64_t intd;
        double reald;
        std::string strd;
    };

    struct central_repo_entry {
        std::uint32_t key;
        central_repo_entry_variant data;
        std::uint32_t metadata_val;
        bool transient = false;
    };

    class central_repo_client;

    enum class central_repo_transaction_mode : std::uint8_t {
        none = 0,
        read = 1,
        concurrent_read_write = 2,
        read_write = 3
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

    struct central_repo_client_subsession;

    struct central_repo {
        drive_number reside_place;

        std::uint32_t access_count;

        // TODO (pent0): Add read/write cap
        std::uint8_t ver;
        std::uint8_t keyspace_type;
        std::uint32_t uid;

        std::uint32_t owner_uid;

        std::vector<central_repo_entry> entries;
        std::vector<central_repo_client_subsession *> attached;

        central_repo_entry_access_policy default_policy;
        std::vector<central_repo_entry_access_policy> single_policies;
        std::vector<central_repo_entry_access_policy> policies_range;

        std::uint32_t default_meta = 0;
        std::vector<central_repo_default_meta> meta_range;

        std::uint64_t time_stamp;

        std::vector<std::uint32_t> deleted_settings;

        bool write_changes(eka2l1::io_system *io, device_manager *mngr, bool atomic = false);
        central_repo_entry *find_entry(const std::uint32_t key);

        std::uint32_t get_default_meta_for_new_key(const std::uint32_t key);

        bool add_new_entry(const std::uint32_t key, const central_repo_entry_variant &var);
        bool add_new_entry(const std::uint32_t key, const central_repo_entry_variant &var,
            const std::uint32_t meta);

        /**
         * \brief Query all entries that match given bit pattern.
         * 
         * The mask marks bit which is mandatory, and partial key is the pattern to be matched.
         * 
         * For example, let say we have these keys:
         * 
         * 0x02B30B11                       0x07B10B52
         * 
         * If the mask is 0xF0FF0000, these will be ignored.
         * 
         * 0x02B30B11                       0x07B10B52
         *    ^  ^^^^                          ^  ^^^^
         *    i  iiii                          i  iiii
         * 
         * Than, the partial key comes in, which is the pattern for the rest of the available bits
         * to be matched. If partial key = 0x03B100000. (v is match, x is not match, i is ignored)
         * 
         * 0x0-B3----                       0x0-B1----
         *   vivxiiii                         vivviiii
         * 0x03B10000                       0x03B10000
         * 
         * As you can see, the one that match our description is 0x07B10B52
         * 
         * \param partial_key     The bit pattern to be matched.
         * \param mask            The mask that requires which bit is mandatory.
         * \param matched_entries Reference to vector containing entries.
         * \param etype           The type of all the entries to be matched.
         */
        void query_entries(const std::uint32_t partial_key, const std::uint32_t mask,
            std::vector<central_repo_entry *> &matched_entries,
            const central_repo_entry_type etype);
    };

    struct central_repo_client_subsession;

    /*! \brief A repos cacher
     *
     * This cacher are likely to be used to store original backup repo.
     * Once the map reach its limit, the oldest one got removed.
    */
    struct central_repos_cacher {
        struct cache_entry {
            std::uint64_t last_access;
            eka2l1::central_repo repo;
        };

        // TODO: Modifable value
        enum {
            MAX_REPO_CACHE_ENTRIES = 35
        };

        std::unordered_map<std::uint32_t, cache_entry> entries;

        bool free_oldest();

        eka2l1::central_repo *add_repo(const std::uint32_t key, eka2l1::central_repo &repo);
        bool remove_repo(const std::uint32_t key);

        // USE ONE TIME ONLY!
        eka2l1::central_repo *get_cached_repo(const std::uint32_t key);
    };

    struct central_repo_key_filter {
        std::uint32_t partial_key;
        std::uint32_t id_mask;
    };

    struct central_repo_client_subsession {
        central_repo_server *server;

        central_repo *attach_repo;
        std::unordered_map<std::uint32_t, central_repo_entry> transaction_entries;
        central_repo_transaction_mode transaction_mode = central_repo_transaction_mode::none;
        int transaction_error = 0;
        std::uint32_t transaction_error_key = 0xFFFFFFFF;

        std::vector<std::uint32_t> key_found_result;

        int reset_key(eka2l1::central_repo *init_repo, const std::uint32_t key);
        void write_changes(eka2l1::io_system *io, device_manager *mngr);

        void find(service::ipc_context *ctx);
        void reset(service::ipc_context *ctx);
        void set_value(service::ipc_context *ctx);
        void get_value(service::ipc_context *ctx);
        void notify_nof_check(service::ipc_context *ctx);
        void notify(service::ipc_context *ctx);
        void notify_cancel(service::ipc_context *ctx);
        void get_find_result(service::ipc_context *ctx);
        void start_transaction(service::ipc_context *ctx);
        void commit_transaction(service::ipc_context *ctx);
        void cancel_transaction(service::ipc_context *ctx);
        void create_value(service::ipc_context *ctx);

        void handle_message(service::ipc_context *ctx);

        struct cenrep_notify_info {
            epoc::notify_info sts;

            std::uint32_t mask;
            std::uint32_t match;
        };

        std::vector<cenrep_notify_info> notifies;

        bool is_active() const {
            return transaction_mode != central_repo_transaction_mode::none;
        }

        central_repo_entry *get_entry(std::uint32_t key);
        void set_entry(service::ipc_context *ctx, bool create);
        int prepare_write();
        void clear_transaction();
        void modification_success(std::uint32_t key, bool committing = false);

        /*! \brief Request to notify when a group of key have changed.
         *
         * \param info  Info of the notify request
         * \param mask  The mask to extract bit pattern
         * \param match The bit pattern to be match.
         * 
         * \returns     0 if success.
         *              -1 if request already exists
        */
        int add_notify_request(epoc::notify_info &info, const std::uint32_t mask, const std::uint32_t match);

        /**
         * \brief Cancel all watches.
         */
        void cancel_all_notify_requests();

        /**
         * \brief Cancel all watches to the given key.
         * 
         * \param match_key The key to erase notify requests.
         */
        void cancel_notify_request(const std::uint32_t match_key, const std::uint32_t mask);
    };
}
