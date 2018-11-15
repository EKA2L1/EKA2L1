#pragma once

#include <core/services/server.h>

#include <map>
#include <string>
#include <unordered_map>

#define CENTRAL_REPO_UID_STRING "1020be9"

namespace eka2l1 {
	enum {
		CENTRAL_REPO_UID = 0x10202be9
	};

	enum class central_repo_entry_type {
		none,
		integer,
		real,
		string8,
		string16
	};

	struct central_repo_entry_variant {
        central_repo_entry_type etype;

		union {
            std::uint64_t intd;
            double reald;
            std::string strd;
            std::u16string str16d;
		};
	};

	struct central_repo_entry {
        std::size_t key;
        central_repo_entry_variant data;
        std::uint32_t metadata_val;
	};

	class central_repo_client;

	enum class central_repo_transaction_mode {
		read_only,
		write_only,
		read_write
	};

	struct central_repo {
		// TODO (pent0): Add read/write cap
        std::size_t owner;
		std::unordered_map<std::size_t, central_repo_entry> entries;
	};

	enum class central_repo_srv_request {
		init,
		create_int,
		create_real,
		create_string,
		delet,
		get_int,
		set_int,
		get_string,
		set_string,
		find,
		find_eq_int,
		find_rq_real,
		find_eq_string,
		find_neq_int,
		find_neq_real,
		find_neq_string,
		get_find_res,
		notify_req_check,
		notify_req,
		notify_cancel,
		notify_cancel_all,
		group_nof_req,
		group_nof_cancel,
		reset,
		reset_all,
		transaction_start,
		transaction_commit,
		transaction_cancel,
		move,
		transaction_state,
		transaction_fail,
		delete_range,
		get_meta,
		close
	};

	struct central_repo_client_session {
        central_repo *attach_repo;
        central_repo_transaction_mode lock_mode;
	};

	class central_repo_server : public service::server {
		// Cached repos. The key is the owner of the repo.
		std::unordered_map<std::uint32_t, central_repo> repos;
        std::map<std::uint32_t, central_repo_client_session> client_sessions;
	};
}