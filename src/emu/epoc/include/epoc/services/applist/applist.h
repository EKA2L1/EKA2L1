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

#include <epoc/services/server.h>
#include <epoc/utils/des.h>

namespace eka2l1 {
    struct apa_app_info {
        std::uint32_t uid; ///< The UID of the application.
        epoc::filename app_path; ///< The path to the application DLL (EKA1) / EXE (EKA2)
        epoc::apa_app_caption short_caption; ///< Short version of the caption
        epoc::apa_app_caption long_caption; ///< Long caption of the app

        apa_app_info() {}
    };

    /*! \brief Struct describles the high-level usage of a application for the
     *         APA framework.
    */
    struct apa_capability {
        /*! \brief The embeddability of an app.
         *
         * This was used during EKA1.
        */
        enum class embeddability {
            not_embeddable,
            embeddable,
            embeddable_only,
            embeddable_ui_or_standalone = 5,
            embeddable_ui_not_standalone
        };

        /*! \brief The attribute of the capability.
        */
        enum capability_attrib {
            built_as_dll = 1,
            control_panel_item = 2,
            non_native = 4
        };

        embeddability ability;

        // If the app supports create new file if being asked (for example, document application
        // creates new .doc file), this will be checked.
        bool support_being_asked_to_create_new_file;

        // If the application is not visible, this will be marked.
        bool is_hidden;
        bool launch_in_background;

        // If you have use Symbian 9.x before, you can put apps and games into folders
        // The name of the folder is the group name.
        epoc::buf_static<char16_t, 0x10> group_name;
        std::uint32_t flags{ 0 };

        int reserved;

        apa_capability() {}
    };

    /*! \brief Applist services
     *
     * Provide external information about application management,
     * and HAL information regards to each app.
	 *
	 * Disable for LLE testings.
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

        /*! \brief Request the server to run app.
         *
         * Iter through every AppList plugins, set status to true
         * if application is allowed to run
         * 
         * \param arg0: App's uid3
        */
        void is_accepted_to_run(service::ipc_context ctx);

        /*! \brief Get the info of the specified application
         *
         * \param arg0: App's uid3
         * \param arg1: Descriptor contains apa_app_info
        */
        void get_app_info(service::ipc_context ctx);

        /*! \brief Get the capability of an app.
         *
         * \param arg0: Descriptor contains the capability
         * \param arg1: The application's UID
        */
        void get_capability(service::ipc_context ctx);

    public:
        applist_server(system *sys);
    };
}