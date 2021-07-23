#include <qt/utils.h>

#include <config/app_settings.h>

#include <system/epoc.h>
#include <kernel/kernel.h>
#include <services/window/window.h>
#include <services/window/classes/wingroup.h>
#include <services/ui/cap/oom_app.h>

#include <QSettings>
#include <QCheckBox>

eka2l1::window_server *get_window_server_through_system(eka2l1::system *sys) {
    eka2l1::kernel_system *kernel = sys->get_kernel_system();
    if (!kernel) {
        return nullptr;
    }

    const std::string win_server_name = eka2l1::get_winserv_name_by_epocver(kernel->get_epoc_version());
    return reinterpret_cast<eka2l1::window_server*>(kernel->get_by_name<eka2l1::service::server>(win_server_name));
}

eka2l1::epoc::screen *get_current_active_screen(eka2l1::system *sys, const int provided_num) {
    int active_screen_number = provided_num;
    if (provided_num < 0) {
        QSettings settings;
        active_screen_number = settings.value(SHOW_SCREEN_NUMBER_SETTINGS_NAME, 0).toInt();
    }

    eka2l1::window_server *server = get_window_server_through_system(sys);
    if (server) {
        eka2l1::epoc::screen *scr = server->get_screens();
        while (scr) {
            if (scr->number == active_screen_number) {
                return scr;
            }

            scr = scr->next;
        }
    }

    return nullptr;
}

std::optional<eka2l1::akn_running_app_info> get_active_app_info(eka2l1::system *sys, const int provided_num) {
    eka2l1::epoc::screen *scr = get_current_active_screen(sys, provided_num);
    if (!scr || !scr->focus) {
        return std::nullopt;
    }

    eka2l1::epoc::window_group *group = scr->get_group_chain();
    std::optional<eka2l1::akn_running_app_info> best_info;

    while (group) {
        std::optional<eka2l1::akn_running_app_info> info = eka2l1::get_akn_app_info_from_window_group(group);
        if (info.has_value()) {
            if (group == scr->focus) {
                return info;
            }

            eka2l1::kernel::thread *own_thr = scr->focus->client->get_client();
            if (own_thr && (info->associated_->has_child_process(own_thr->owning_process()))) {
                best_info = info;
            }
        }

        group = reinterpret_cast<eka2l1::epoc::window_group*>(group->sibling);
    }

    return best_info;
}

eka2l1::config::app_setting *get_active_app_setting(eka2l1::system *sys, eka2l1::config::app_settings &settings, const int provided_num) {
    std::optional<eka2l1::akn_running_app_info> info = get_active_app_info(sys, provided_num);
    if (!info.has_value()) {
        return nullptr;
    }
    return settings.get_setting(info->app_uid_);
}

QMessageBox::StandardButton make_dialog_with_checkbox_and_choices(const QString &title, const QString &text, const QString &checkbox_text, const bool checkbox_state, dialog_checkbox_toggled_callback checkbox_callback, const bool two_choices) {
    QCheckBox *checkbox = new QCheckBox(checkbox_text);
    QMessageBox dialog;
    dialog.setWindowTitle(title);
    dialog.setText(text);
    dialog.setCheckBox(checkbox);
    dialog.setIcon(QMessageBox::Information);
    dialog.addButton(QMessageBox::Ok);

    if (two_choices)
        dialog.addButton(QMessageBox::Cancel);

    dialog.setDefaultButton(QMessageBox::Ok);
    checkbox->setChecked(checkbox_state);

    QObject::connect(checkbox, &QCheckBox::toggled, checkbox_callback);

    dialog.exec();
    return static_cast<QMessageBox::StandardButton>(dialog.result());
}
