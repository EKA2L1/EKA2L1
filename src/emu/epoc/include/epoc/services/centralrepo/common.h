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

namespace eka2l1 {
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

    constexpr int cenrep_pma_ver = 3;
}