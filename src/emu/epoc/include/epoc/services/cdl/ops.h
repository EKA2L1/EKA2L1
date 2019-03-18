/*
 * Copyright (c) 2019 EKA2L1 Team.
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

namespace eka2l1::epoc {
    /**
     * @brief CDL Server Opcodes
     */
    enum cdl_server_cmd {
        cdl_server_cmd_request_get_cust,
        cdl_server_cmd_get_cust,
        cdl_server_cmd_set_uids_to_notify,
        cdl_server_cmd_notify_change,
        cdl_server_cmd_cancel_notify_change,
        cdl_server_cmd_set_cust,
        cdl_server_cmd_get_cust_size,
        cdl_server_cmd_get_name_size,
        cdl_server_cmd_get_temp_buf,
        cdl_server_cmd_get_all_refs_size,
        cdl_server_cmd_is_plugin_in_rom,
        cdl_server_cmd_plugin_drive,
    };
}
