/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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
     * \brief Error codes used by the ECOM server to report to its client.
     */
    enum ecom_error_code {
        /**
         * Given interface UID was not found on the server.
         */
        ecom_no_interface_identified = -17004,

        /**
         * No implementation found on the server.
         */
        ecom_no_registeration_identified = -17007
    };

    enum {
        ecom_interface_uid_index = 0,
        ecom_impl_uid_index = 0,
        ecom_dtor_uid_index = 1,
        ecom_resolver_uid_index = 2
    };
}
