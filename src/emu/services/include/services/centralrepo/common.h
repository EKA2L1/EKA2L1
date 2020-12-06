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

#include <cstdint>

namespace eka2l1 {
    enum central_repo_srv_request {
        cen_rep_init,
        cen_rep_create_int,
        cen_rep_create_real,
        cen_rep_create_string,
        cen_rep_delete,
        cen_rep_get_int,
        cen_rep_set_int,
        cen_rep_get_real,
        cen_rep_set_real,
        cen_rep_get_string,
        cen_rep_set_string,
        cen_rep_find,
        cen_rep_find_eq_int,
        cen_rep_find_eq_real,
        cen_rep_find_eq_string,
        cen_rep_find_neq_int,
        cen_rep_find_neq_real,
        cen_rep_find_neq_string,
        cen_rep_get_find_res,
        cen_rep_notify_req_check,
        cen_rep_notify_req,
        cen_rep_notify_cancel,
        cen_rep_notify_cancel_all,
        cen_rep_group_nof_req,
        cen_rep_group_nof_cancel,
        cen_rep_reset,
        cen_rep_reset_all,
        cen_rep_transaction_start,
        cen_rep_transaction_commit,
        cen_rep_transaction_cancel,
        cen_rep_move,
        cen_rep_transaction_state,
        cen_rep_transaction_fail,
        cen_rep_delete_range,
        cen_rep_get_meta,
        cen_rep_close
    };

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

    static constexpr int cenrep_pma_ver = 3;
    static constexpr std::uint32_t MAX_FOUND_UID_BUF_LENGTH = 16;
}