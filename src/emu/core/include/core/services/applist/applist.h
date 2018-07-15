/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <services/server.h>

namespace eka2l1 {
    /*! \brief Applist services
     *
     * Provide external information about application management,
     * and HAL information regards to each app.
     */
    class applist_server : public service::server {
        /*! \brief Get the number of screen shared for an app. 
         * 
         * \param arg0: pointer to the number of screen.
         * \param arg1: application UID.
         * \param request_sts: KErrNotFound if app doesn't exist.
        */
        void default_screen_number(service::ipc_context ctx);

        /*! \brief Get the application language.
         *
         * \param arg0: App's uid3.
         * \param arg1: App language.
         *
         * Expected request status: KErrNone. 
        */
        void app_language(service::ipc_context ctx);

    public:
        applist_server(system *sys);
    };
}