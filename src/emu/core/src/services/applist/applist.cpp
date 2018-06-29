#include <services/applist/applist.h>
#include <services/applist/op.h>

#include <core.h>
#include <functional>

#include <e32lang.h>

namespace eka2l1 {
    applist_server::applist_server(system *sys)
        : service::server(sys, "!AppListServer") {
        REGISTER_IPC(applist_server, default_screen_number,
            EAppListServGetDefaultScreenNumber, "GetDefaultScreenNumber");
        REGISTER_IPC(applist_server, app_language,
            EAppListServApplicationLanguage, "ApplicationLanguageL");
    }

    /*! \brief Get the number of screen shared for an app. 
     * 
     * arg0: pointer to the number of screen
     * arg1: application UID
     * request_sts: KErrNotFound if app doesn't exist
    */
    void applist_server::default_screen_number(service::ipc_context ctx) {
        eka2l1::ptr<int> number = *ctx.get_arg<int>(0);
        system *tsys = ctx.sys;

        *number.get(tsys->get_memory_system()) = 1;
        ctx.set_request_status(0); // KErrNone
    }

    void applist_server::app_language(service::ipc_context ctx) {
        ctx.write_arg_pkg<TLanguage>(1, ELangEnglish);
        ctx.set_request_status(0);
    }
}