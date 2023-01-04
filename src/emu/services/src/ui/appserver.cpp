#include <services/ui/appserver.h>
#include <utils/err.h>

namespace eka2l1 {
    app_ui_based_session::app_ui_based_session(service::typical_server *serv, kernel::uid client_ss_uid, epoc::version client_version)
        : service::typical_session(serv, client_ss_uid, client_version)
        , exit_reason_(epoc::status_pending) {

    }

    app_ui_based_session::~app_ui_based_session() {
        if (!notify_exit_.empty() && (exit_reason_ != epoc::status_pending)) {
            notify_exit_.complete(exit_reason_);
        }
    }

    void app_ui_based_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case app_ui_based_notify_app_exit: {
            if (!notify_exit_.empty()) {
                ctx->complete(epoc::error_in_use);
                return;
            }

            notify_exit_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
            break;
        }

        case app_ui_based_cancel_notify_app_exit: {
            notify_exit_.complete(epoc::error_cancel);
            break;
        }

        default:
            LOG_ERROR(SERVICE_UI, "Unhandled app ui based session opcode {}", ctx->msg->function);
            break;
        }
    }

    void app_ui_based_session::complete_server_exit_notify(int error_code) {
        if (exit_reason_ == epoc::status_pending) {
            exit_reason_ = error_code;
        }
    }

    app_ui_based_server::app_ui_based_server(system *sys, std::uint32_t server_differentiator, std::uint32_t app_uid)
        : service::typical_server(sys, fmt::format("{:x}_{:x}_AppServer", server_differentiator, app_uid)) {

    }

    app_ui_based_server::~app_ui_based_server() {
        // TODO: Use owning process exit status code instead
        notify_app_exit(epoc::error_none);
    }

    void app_ui_based_server::notify_app_exit(int error_code) {
        for (auto &session: sessions) {
            reinterpret_cast<app_ui_based_session*>(session.second.get())->complete_server_exit_notify(error_code);
        }
    }
}