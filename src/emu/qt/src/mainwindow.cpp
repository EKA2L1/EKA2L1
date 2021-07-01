#include <qt/applistwidget.h>
#include <qt/mainwindow.h>
#include <qt/aboutdialog.h>
#include <qt/state.h>
#include "./ui_mainwindow.h"

#include <system/epoc.h>
#include <kernel/kernel.h>

#include <services/applist/applist.h>
#include <services/fbs/fbs.h>

main_window::main_window(QWidget *parent, eka2l1::desktop::emulator &emulator_state)
    : QMainWindow(parent)
    , emulator_state_(emulator_state)
    , ui_(new Ui::main_window)
    , applist_(nullptr)
{
    ui_->setupUi(this);
    ui_->label_al_not_available->setVisible(false);

    eka2l1::system *system = emulator_state_.symsys.get();
    if (system) {
        eka2l1::kernel_system *kernel = system->get_kernel_system();
        if (kernel) {
            const std::string al_server_name = eka2l1::get_app_list_server_name_by_epocver(kernel->get_epoc_version());
            const std::string fbs_server_name = eka2l1::epoc::get_fbs_server_name_by_epocver(kernel->get_epoc_version());

            eka2l1::applist_server *al_serv = reinterpret_cast<eka2l1::applist_server*>(kernel->get_by_name<eka2l1::service::server>(al_server_name));
            eka2l1::fbs_server *fbs_serv = reinterpret_cast<eka2l1::fbs_server*>(kernel->get_by_name<eka2l1::service::server>(fbs_server_name));

            if (al_serv && fbs_serv) {
                applist_ = new applist_widget(this, al_serv, fbs_serv);
                ui_->layout_main->addWidget(applist_);
            }
        }
    }

    if (!applist_) {
        ui_->label_al_not_available->setVisible(true);
    }

    connect(ui_->action_about, &QAction::triggered, this, &main_window::on_about_triggered);
}

main_window::~main_window()
{
    if (applist_) {
        delete applist_;
    }

    delete ui_;
}

void main_window::on_about_triggered() {
    about_dialog *aboutDiag = new about_dialog(this);
    aboutDiag->show();
}
