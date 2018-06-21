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
         * arg0: pointer to the number of screen
         * arg1: application UID
         * request_sts: KErrNotFound if app doesn't exist
        */
        void default_screen_number(service::ipc_context ctx);

    public:
        applist_server(system *sys);
    };
}